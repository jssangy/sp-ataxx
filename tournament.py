#!/usr/bin/env python3
"""
OctaFlip Tournament - Maximum Stability Version
"""

import subprocess
import time
import os
import signal
import sys
import re
import socket

class RobustTournament:
    def __init__(self):
        self.engines = {
            1: "Eunsong",
            2: "MCTS", 
            3: "AlphaBeta",
            4: "TournamentBeast",
            5: "UltimateAI",
            6: "HybridMCTS"
        }
        
        # 실행 파일 확인
        if not os.path.exists("./server") or not os.path.exists("./client"):
            print("ERROR: server and client executables not found!")
            print("Please run 'make' first.")
            sys.exit(1)
        
        # 로그 디렉토리
        os.makedirs("tournament_logs", exist_ok=True)
        
        # 디버그 모드
        self.debug = False
    
    def cleanup_everything(self):
        """완전한 정리"""
        print("Cleaning up...", end='', flush=True)
        
        # 모든 서버/클라이언트 프로세스 종료
        os.system("killall -9 server 2>/dev/null")
        os.system("killall -9 client 2>/dev/null")
        
        # 추가 정리
        os.system("pkill -9 -f './server' 2>/dev/null")
        os.system("pkill -9 -f './client' 2>/dev/null")
        
        time.sleep(2)
        print(" done")
    
    def is_port_free(self, port=8080):
        """포트가 사용 가능한지 확인"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(('', port))
            sock.close()
            return True
        except:
            return False
    
    def wait_for_port(self, port=8080, timeout=5):
        """포트가 열릴 때까지 대기"""
        start = time.time()
        while time.time() - start < timeout:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            result = sock.connect_ex(('127.0.0.1', port))
            sock.close()
            if result == 0:
                return True
            time.sleep(0.1)
        return False
    
    def run_game_simple(self, red_engine_id, blue_engine_id, game_num):
        """단순하고 안정적인 게임 실행"""
        print(f"\n{'='*60}")
        print(f"Game #{game_num}: {self.engines[red_engine_id]} (Red) vs {self.engines[blue_engine_id]} (Blue)")
        print(f"{'='*60}")
        
        # 완전한 정리부터 시작
        self.cleanup_everything()
        
        # 포트 확인
        if not self.is_port_free(8080):
            print("Port 8080 is busy. Waiting...")
            time.sleep(3)
            self.cleanup_everything()
            time.sleep(2)
        
        log_file = f"tournament_logs/game_{game_num}_{red_engine_id}v{blue_engine_id}.log"
        
        try:
            # 1. 서버 시작
            print("Starting server...", end='', flush=True)
            server_proc = subprocess.Popen(
                ["./server"],
                stdout=open(log_file, 'w'),
                stderr=subprocess.STDOUT
            )
            
            # 서버가 포트를 열 때까지 대기
            if not self.wait_for_port(8080, 5):
                print(" FAILED (port not open)")
                server_proc.kill()
                return None
            
            # 서버가 완전히 준비될 때까지 추가 대기
            time.sleep(2)
            print(" OK")
            
            # 2. Red 플레이어 시작
            print(f"Starting {self.engines[red_engine_id]} (Red)...", end='', flush=True)
            
            client1_proc = subprocess.Popen(
                ["./client", 
                 "-ip", "127.0.0.1",
                 "-port", "8080",
                 "-username", f"Engine{red_engine_id}",
                 "-engine", str(red_engine_id),
                 "-no-board"],
                stdout=subprocess.DEVNULL if not self.debug else None,
                stderr=subprocess.DEVNULL if not self.debug else None
            )
            
            # 첫 번째 클라이언트가 연결되고 등록할 시간
            time.sleep(3)
            
            # 프로세스 확인
            if client1_proc.poll() is not None:
                print(" FAILED (process died)")
                server_proc.kill()
                return None
            print(" OK")
            
            # 3. Blue 플레이어 시작
            print(f"Starting {self.engines[blue_engine_id]} (Blue)...", end='', flush=True)
            
            client2_proc = subprocess.Popen(
                ["./client",
                 "-ip", "127.0.0.1", 
                 "-port", "8080",
                 "-username", f"Engine{blue_engine_id}",
                 "-engine", str(blue_engine_id),
                 "-no-board"],
                stdout=subprocess.DEVNULL if not self.debug else None,
                stderr=subprocess.DEVNULL if not self.debug else None
            )
            
            # 두 번째 클라이언트 연결 대기
            time.sleep(2)
            
            # 프로세스 확인
            if client2_proc.poll() is not None:
                print(" FAILED (process died)")
                server_proc.kill()
                client1_proc.kill()
                return None
            print(" OK")
            
            # 4. 게임 진행
            print("\nGame running", end='', flush=True)
            
            timeout = 90  # 1.5분
            start = time.time()
            
            while time.time() - start < timeout:
                # 서버 종료 확인
                if server_proc.poll() is not None:
                    print("\nGame finished!")
                    break
                
                # 클라이언트 상태 확인
                if client1_proc.poll() is not None and client2_proc.poll() is not None:
                    print("\nClients disconnected")
                    time.sleep(2)
                    break
                
                print(".", end='', flush=True)
                time.sleep(3)
            else:
                print("\nTimeout!")
            
            # 5. 정리
            time.sleep(1)
            
            # 프로세스 종료
            for proc in [client1_proc, client2_proc, server_proc]:
                try:
                    proc.terminate()
                    time.sleep(0.5)
                    if proc.poll() is None:
                        proc.kill()
                except:
                    pass
            
            time.sleep(2)
            
            # 6. 결과 파싱
            return self.parse_result(log_file)
            
        except Exception as e:
            print(f"\nERROR: {e}")
            if self.debug:
                import traceback
                traceback.print_exc()
            return None
        
        finally:
            # 완전한 정리
            self.cleanup_everything()
    
    def parse_result(self, log_file):
        """결과 파싱"""
        try:
            with open(log_file, 'r') as f:
                content = f.read()
            
            # 점수 찾기
            match = re.search(r"Red:\s*(\d+),\s*Blue:\s*(\d+)", content)
            if match:
                red = int(match.group(1))
                blue = int(match.group(2))
                
                print(f"Score: Red {red} - Blue {blue}")
                
                if red > blue:
                    return "RED", red, blue
                elif blue > red:
                    return "BLUE", red, blue
                else:
                    return "DRAW", red, blue
            
            print("Could not find score in log")
            return None
            
        except Exception as e:
            print(f"Parse error: {e}")
            return None
    
    def double_round_robin(self):
        """더블 라운드 로빈"""
        print("\n" + "="*70)
        print("Double Round Robin Tournament")
        print("="*70)
        
        results = {}
        for e in self.engines:
            results[e] = {
                "wins": 0,
                "losses": 0,
                "draws": 0,
                "points": 0,
                "for": 0,
                "against": 0
            }
        
        game_count = 0
        successful_games = 0
        
        # 모든 쌍에 대해
        for e1 in range(1, 7):
            for e2 in range(e1 + 1, 7):
                # 첫 번째 게임: e1이 Red
                game_count += 1
                print(f"\n[Game {game_count}/30]")
                
                result = self.run_game_simple(e1, e2, game_count)
                
                if result:
                    successful_games += 1
                    winner, red_score, blue_score = result
                    
                    results[e1]["for"] += red_score
                    results[e1]["against"] += blue_score
                    results[e2]["for"] += blue_score
                    results[e2]["against"] += red_score
                    
                    if winner == "RED":
                        results[e1]["wins"] += 1
                        results[e1]["points"] += 3
                        results[e2]["losses"] += 1
                        print(f"Winner: {self.engines[e1]}")
                    elif winner == "BLUE":
                        results[e2]["wins"] += 1
                        results[e2]["points"] += 3
                        results[e1]["losses"] += 1
                        print(f"Winner: {self.engines[e2]}")
                    else:
                        results[e1]["draws"] += 1
                        results[e1]["points"] += 1
                        results[e2]["draws"] += 1
                        results[e2]["points"] += 1
                        print("Draw!")
                else:
                    print("Game failed!")
                
                # 충분한 휴식
                time.sleep(5)
                
                # 두 번째 게임: e2가 Red
                game_count += 1
                print(f"\n[Game {game_count}/30]")
                
                result = self.run_game_simple(e2, e1, game_count)
                
                if result:
                    successful_games += 1
                    winner, red_score, blue_score = result
                    
                    results[e2]["for"] += red_score
                    results[e2]["against"] += blue_score
                    results[e1]["for"] += blue_score
                    results[e1]["against"] += red_score
                    
                    if winner == "RED":
                        results[e2]["wins"] += 1
                        results[e2]["points"] += 3
                        results[e1]["losses"] += 1
                        print(f"Winner: {self.engines[e2]}")
                    elif winner == "BLUE":
                        results[e1]["wins"] += 1
                        results[e1]["points"] += 3
                        results[e2]["losses"] += 1
                        print(f"Winner: {self.engines[e1]}")
                    else:
                        results[e1]["draws"] += 1
                        results[e1]["points"] += 1
                        results[e2]["draws"] += 1
                        results[e2]["points"] += 1
                        print("Draw!")
                else:
                    print("Game failed!")
                
                # 충분한 휴식
                time.sleep(5)
        
        # 결과 출력
        print("\n" + "="*70)
        print(f"Tournament Complete! ({successful_games}/30 games successful)")
        print("="*70)
        
        # 순위표
        sorted_engines = sorted(
            results.items(),
            key=lambda x: (x[1]["points"], x[1]["wins"], x[1]["for"] - x[1]["against"]),
            reverse=True
        )
        
        print(f"\n{'Rank':<6} {'Engine':<15} {'Played':<8} {'W':<4} {'D':<4} {'L':<4} "
              f"{'Pts':<5} {'For':<5} {'Agn':<5} {'Diff':<6}")
        print("-" * 70)
        
        for rank, (e_id, stats) in enumerate(sorted_engines, 1):
            played = stats["wins"] + stats["draws"] + stats["losses"]
            diff = stats["for"] - stats["against"]
            diff_str = f"+{diff}" if diff >= 0 else str(diff)
            
            print(f"{rank:<6} {self.engines[e_id]:<15} {played:<8} "
                  f"{stats['wins']:<4} {stats['draws']:<4} {stats['losses']:<4} "
                  f"{stats['points']:<5} {stats['for']:<5} {stats['against']:<5} "
                  f"{diff_str:<6}")
    
    def run_match(self, e1, e2):
        """두 엔진 간 매치"""
        print(f"\nMatch: {self.engines[e1]} vs {self.engines[e2]}")
        
        stats = {e1: 0, e2: 0, "draws": 0}
        
        # Game 1
        result = self.run_game_simple(e1, e2, 1)
        if result:
            winner, _, _ = result
            if winner == "RED":
                stats[e1] += 1
            elif winner == "BLUE":
                stats[e2] += 1
            else:
                stats["draws"] += 1
        
        time.sleep(5)
        
        # Game 2
        result = self.run_game_simple(e2, e1, 2)
        if result:
            winner, _, _ = result
            if winner == "RED":
                stats[e2] += 1
            elif winner == "BLUE":
                stats[e1] += 1
            else:
                stats["draws"] += 1
        
        print(f"\nMatch Result:")
        print(f"{self.engines[e1]}: {stats[e1]} wins")
        print(f"{self.engines[e2]}: {stats[e2]} wins")
        print(f"Draws: {stats['draws']}")


def main():
    tournament = RobustTournament()
    
    # 시작 전 정리
    tournament.cleanup_everything()
    
    while True:
        print("\n" + "="*50)
        print("OctaFlip Tournament (Robust)")
        print("="*50)
        print("1. Double Round Robin")
        print("2. Two Engine Match")
        print("3. Debug Mode On/Off")
        print("4. Exit")
        
        choice = input("\nSelect: ")
        
        if choice == '1':
            tournament.double_round_robin()
        elif choice == '2':
            print("\nEngines:")
            for i, name in tournament.engines.items():
                print(f"  {i}. {name}")
            try:
                e1 = int(input("Engine 1: "))
                e2 = int(input("Engine 2: "))
                if e1 in tournament.engines and e2 in tournament.engines and e1 != e2:
                    tournament.run_match(e1, e2)
            except:
                print("Invalid input")
        elif choice == '3':
            tournament.debug = not tournament.debug
            print(f"Debug mode: {'ON' if tournament.debug else 'OFF'}")
        elif choice == '4':
            tournament.cleanup_everything()
            break


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nCleaning up...")
        t = RobustTournament()
        t.cleanup_everything()