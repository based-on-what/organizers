#!/usr/bin/env python3
"""
Enhanced Steam Game Duration Analyzer

This script fetches Steam game libraries and analyzes completion times using
HowLongToBeat data, providing organized results by game completion duration.
"""

import sys
from pathlib import Path
from typing import Dict, Optional

try:
    import requests
except ImportError:
    print("Error: requests is required. Install with: pip install requests")
    sys.exit(1)

try:
    from howlongtobeatpy import HowLongToBeat
except ImportError:
    print("Error: howlongtobeatpy is required. Install with: pip install howlongtobeatpy")
    sys.exit(1)

from shared_utils import setup_logging, save_results_to_file


class SteamGameAnalyzer:
    """Steam game library analyzer with HowLongToBeat integration."""
    
    def __init__(self, api_key: str):
        """
        Initialize the analyzer with Steam API key.
        
        Args:
            api_key: Steam Web API key
        """
        self.api_key = api_key
        self.hltb = HowLongToBeat()
        
    def get_owned_games(self, steam_id: str, include_appinfo: bool = True) -> Optional[Dict]:
        """
        Fetch owned games from Steam API.
        
        Args:
            steam_id: Steam user ID
            include_appinfo: Whether to include app info (names, etc.)
            
        Returns:
            Dictionary containing games data or None if error
        """
        url = (
            f"http://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/"
            f"?key={self.api_key}&steamid={steam_id}&format=json"
            f"&include_appinfo={'1' if include_appinfo else '0'}"
        )
        
        try:
            response = requests.get(url, timeout=30)
            response.raise_for_status()
            
            data = response.json()
            return data.get('response', {})
            
        except requests.exceptions.RequestException as e:
            logging.error(f"Error fetching Steam data for {steam_id}: {e}")
            return None
        except ValueError as e:
            logging.error(f"Invalid JSON response for {steam_id}: {e}")
            return None
    
    def get_game_completion_time(self, game_name: str) -> Optional[float]:
        """
        Get main story completion time for a game.
        
        Args:
            game_name: Name of the game
            
        Returns:
            Completion time in hours or None if not found
        """
        try:
            results = self.hltb.search(game_name)
            if results:
                # Get the first result's main story time
                main_story = results[0].main_story
                return main_story if main_story and main_story > 0 else None
            return None
            
        except Exception as e:
            logging.warning(f"Error getting completion time for '{game_name}': {e}")
            return None
    
    def analyze_steam_library(self, steam_ids: list) -> Dict[str, Optional[float]]:
        """
        Analyze multiple Steam libraries and get completion times.
        
        Args:
            steam_ids: List of Steam user IDs
            
        Returns:
            Dictionary mapping game names to completion times
        """
        games_completion_times = {}
        
        for steam_id in steam_ids:
            logging.info(f"Processing Steam library for user: {steam_id}")
            
            games_data = self.get_owned_games(steam_id)
            if not games_data or 'games' not in games_data:
                logging.warning(f"No games data found for Steam ID: {steam_id}")
                continue
            
            games_list = games_data['games']
            logging.info(f"Found {len(games_list)} games in library")
            
            for game in games_list:
                game_name = game.get('name', 'Unknown Game')
                
                # Skip if we already have this game
                if game_name in games_completion_times:
                    continue
                
                completion_time = self.get_game_completion_time(game_name)
                games_completion_times[game_name] = completion_time
                
                if completion_time:
                    logging.info(f"✓ {game_name}: {completion_time:.1f} hours")
                else:
                    logging.warning(f"✗ {game_name}: No completion data found")
        
        return games_completion_times


def save_game_results(games_dict: Dict[str, Optional[float]], output_file: str) -> None:
    """Save game completion times to a file."""
    # Filter out games without completion data and sort by completion time
    valid_games = {name: time for name, time in games_dict.items() if time is not None}
    sorted_games = dict(sorted(valid_games.items(), key=lambda x: x[1]))
    
    results_text = []
    for game_name, completion_time in sorted_games.items():
        results_text.append(f"{game_name}: {completion_time:.1f} hours")
    
    # Add summary
    results_text.extend([
        "",
        f"Total games with completion data: {len(sorted_games)}",
        f"Total games processed: {len(games_dict)}",
        f"Games without data: {len(games_dict) - len(sorted_games)}"
    ])
    
    if sorted_games:
        total_hours = sum(sorted_games.values())
        avg_hours = total_hours / len(sorted_games)
        results_text.extend([
            f"Total completion time: {total_hours:.1f} hours",
            f"Average completion time: {avg_hours:.1f} hours"
        ])
    
    save_results_to_file(results_text, Path(output_file), "STEAM GAMES BY COMPLETION TIME")


def main() -> None:
    """Main function to execute Steam game analysis."""
    # Set up logging
    logger = setup_logging("INFO")
    
    # Configuration - Replace with your actual Steam API key and user IDs
    api_key = ""  # Get from https://steamcommunity.com/dev/apikey
    steam_ids = [
        "",  # Replace with actual Steam IDs
        ""   # Add more Steam IDs as needed
    ]
    
    if not api_key:
        logging.error("Steam API key is required. Get one from https://steamcommunity.com/dev/apikey")
        logging.error("Update the 'api_key' variable in this script.")
        return
    
    if not any(steam_ids):
        logging.error("At least one Steam ID is required.")
        logging.error("Update the 'steam_ids' list in this script.")
        return
    
    # Remove empty Steam IDs
    steam_ids = [sid for sid in steam_ids if sid.strip()]
    
    logging.info(f"Analyzing {len(steam_ids)} Steam libraries...")
    
    # Initialize analyzer
    analyzer = SteamGameAnalyzer(api_key)
    
    # Analyze libraries
    games_completion_times = analyzer.analyze_steam_library(steam_ids)
    
    if not games_completion_times:
        logging.warning("No games found in any of the provided libraries!")
        return
    
    # Save results
    output_file = "steam_games_completion_times.txt"
    save_game_results(games_completion_times, output_file)
    logging.info(f"Analysis complete! Results saved to {output_file}")
    
    # Display summary
    total_games = len(games_completion_times)
    games_with_data = len([g for g in games_completion_times.values() if g is not None])
    
    logging.info(f"\nSummary:")
    logging.info(f"Total games analyzed: {total_games}")
    logging.info(f"Games with completion data: {games_with_data}")
    logging.info(f"Games without data: {total_games - games_with_data}")


if __name__ == "__main__":
    main()