#!/usr/bin/env python3
"""Steam game duration analyzer CLI entry point."""

import logging
import os
from pathlib import Path
from typing import Dict, Optional

from analyzers.steam import SteamClient, HltbClient, analyze_libraries
from core.log import setup_logging
from core.output import save_results_to_file


def save_game_results(games_dict: Dict[str, Optional[float]], output_file: str) -> None:
    valid_games = {name: h for name, h in games_dict.items() if h is not None}
    sorted_games = dict(sorted(valid_games.items(), key=lambda x: x[1]))

    results_text = [f"{name}: {h:.1f} hours" for name, h in sorted_games.items()]
    results_text.extend([
        "",
        f"Total games with completion data: {len(sorted_games)}",
        f"Total games processed: {len(games_dict)}",
        f"Games without data: {len(games_dict) - len(sorted_games)}",
    ])

    if sorted_games:
        total_hours = sum(sorted_games.values())
        avg_hours = total_hours / len(sorted_games)
        results_text.extend([
            f"Total completion time: {total_hours:.1f} hours",
            f"Average completion time: {avg_hours:.1f} hours",
        ])

    save_results_to_file(results_text, Path(output_file), "STEAM GAMES BY COMPLETION TIME")


def main() -> None:
    logger = setup_logging("INFO")

    api_key = os.environ.get("STEAM_API_KEY", "").strip()
    raw_ids = os.environ.get("STEAM_IDS", "").strip()
    steam_ids = [sid.strip() for sid in raw_ids.split(",") if sid.strip()]

    if not api_key:
        logger.error(
            "STEAM_API_KEY env var required. "
            "Get one from https://steamcommunity.com/dev/apikey"
        )
        return

    if not steam_ids:
        logger.error("STEAM_IDS env var required (comma-separated Steam user IDs).")
        return

    logger.info(f"Analyzing {len(steam_ids)} Steam libraries...")

    steam = SteamClient(api_key)
    hltb = HltbClient()
    games_completion_times = analyze_libraries(steam, hltb, steam_ids)

    if not games_completion_times:
        logger.warning("No games found in any of the provided libraries!")
        return

    output_file = "steam_games_completion_times.txt"
    save_game_results(games_completion_times, output_file)
    logger.info(f"Analysis complete! Results saved to {output_file}")

    total_games = len(games_completion_times)
    games_with_data = len([g for g in games_completion_times.values() if g is not None])
    logger.info(f"\nSummary:")
    logger.info(f"Total games analyzed: {total_games}")
    logger.info(f"Games with completion data: {games_with_data}")
    logger.info(f"Games without data: {total_games - games_with_data}")


if __name__ == "__main__":
    main()
