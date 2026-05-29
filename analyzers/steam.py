"""
Steam game library analyzer with HowLongToBeat integration.

Split into three concerns:
  SteamClient   — HTTP calls to Steam Web API (fetching only)
  HltbClient    — HLTB lookups (querying only)
  analyze_libraries() — orchestration: dedup games, rate-limit, collect results
"""
import logging
import sys
import time
from typing import Dict, Optional

_log = logging.getLogger("organizers")

_HLTB_DELAY = 1.0  # seconds between HLTB requests — scraper-style API, be polite

_requests_mod = None
_hltb_cls = None


def _get_requests():
    global _requests_mod
    if _requests_mod is None:
        try:
            import requests
            _requests_mod = requests
        except ImportError:
            _log.error("requests required. Install with: pip install requests")
            sys.exit(1)
    return _requests_mod


def _get_hltb():
    global _hltb_cls
    if _hltb_cls is None:
        try:
            from howlongtobeatpy import HowLongToBeat
            _hltb_cls = HowLongToBeat
        except ImportError:
            _log.error("howlongtobeatpy required. Install with: pip install howlongtobeatpy")
            sys.exit(1)
    return _hltb_cls


class SteamClient:
    """Fetches owned game lists from the Steam Web API. No business logic."""

    def __init__(self, api_key: str):
        self._api_key = api_key
        self._requests = _get_requests()

    def get_owned_games(self, steam_id: str) -> Optional[Dict]:
        """Return the 'response' dict from Steam API, or None on error."""
        url = (
            f"https://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/"
            f"?key={self._api_key}&steamid={steam_id}&format=json&include_appinfo=1"
        )
        try:
            response = self._requests.get(url, timeout=30)
            response.raise_for_status()
            return response.json().get('response', {})
        except self._requests.exceptions.RequestException:
            _log.exception(f"Steam API error for {steam_id}")
            return None
        except ValueError:
            _log.exception(f"Invalid JSON response for {steam_id}")
            return None


class HltbClient:
    """Wraps HowLongToBeat search. Returns raw hours, no formatting."""

    def __init__(self):
        self._hltb = _get_hltb()()

    def get_main_story_hours(self, game_name: str) -> Optional[float]:
        try:
            results = self._hltb.search(game_name)
            if results:
                h = results[0].main_story
                return h if h and h > 0 else None
            return None
        except Exception:
            _log.warning(f"HLTB error for '{game_name}'", exc_info=True)
            return None


def analyze_libraries(
    steam_client: SteamClient,
    hltb_client: HltbClient,
    steam_ids: list,
) -> Dict[str, Optional[float]]:
    """
    Fetch completion hours for all unique games across the given Steam IDs.
    Rate-limited to _HLTB_DELAY between lookups.
    Returns {game_name: hours_or_None}.
    """
    results: Dict[str, Optional[float]] = {}

    for steam_id in steam_ids:
        _log.info(f"Processing Steam library for user: {steam_id}")
        data = steam_client.get_owned_games(steam_id)

        if not data or 'games' not in data:
            _log.warning(f"No games data found for Steam ID: {steam_id}")
            continue

        games = data['games']
        _log.info(f"Found {len(games)} games in library")

        for game in games:
            name = game.get('name', 'Unknown Game')
            if name in results:
                continue

            hours = hltb_client.get_main_story_hours(name)
            results[name] = hours
            time.sleep(_HLTB_DELAY)

            if hours:
                _log.info(f"+ {name}: {hours:.1f} hours")
            else:
                _log.warning(f"- {name}: no completion data")

    return results
