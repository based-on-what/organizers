"""
Steam game library analyzer with HowLongToBeat integration.

Split into four concerns:
  SteamClient   — HTTP calls to Steam Web API (fetching only, with retry)
  HltbClient    — HLTB lookups (querying only)
  HltbCache     — disk cache of HLTB results with TTL, enables fast reruns/resume
  analyze_libraries() — orchestration: dedup games, rate-limit, collect results
"""
import json
import logging
import os
import time
from pathlib import Path
from typing import Dict, Optional

from core.loaders import get_howlongtobeat, get_requests

_log = logging.getLogger("organizers")

_HLTB_DELAY = 1.0  # seconds between HLTB requests — scraper-style API, be polite
_CACHE_TTL_SECONDS = 90 * 24 * 3600
_CACHE_FLUSH_EVERY = 20  # flush to disk every N new lookups so a crash loses little


def default_cache_path() -> Path:
    if os.name == "nt":
        base = os.environ.get("LOCALAPPDATA")
        root = Path(base) if base else Path.home() / ".cache"
    else:
        root = Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache")
    return root / "organizers" / "hltb_cache.json"


class HltbCache:
    """JSON disk cache keyed by game name. Entries expire after ttl_seconds."""

    def __init__(self, path: Path, ttl_seconds: int = _CACHE_TTL_SECONDS):
        self._path = Path(path)
        self._ttl = ttl_seconds
        self._data: Dict[str, Dict] = {}
        self._unsaved = 0
        try:
            raw = json.loads(self._path.read_text(encoding="utf-8"))
            if isinstance(raw, dict):
                self._data = raw
        except (OSError, ValueError):
            pass

    def get(self, name: str):
        """Return (hit, hours). hit is False when absent or expired."""
        entry = self._data.get(name)
        if not isinstance(entry, dict):
            return False, None
        if time.time() - entry.get("fetched_at", 0) > self._ttl:
            return False, None
        return True, entry.get("hours")

    def set(self, name: str, hours: Optional[float]) -> None:
        self._data[name] = {"hours": hours, "fetched_at": time.time()}
        self._unsaved += 1
        if self._unsaved >= _CACHE_FLUSH_EVERY:
            self.save()

    def save(self) -> None:
        try:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            self._path.write_text(
                json.dumps(self._data, ensure_ascii=False), encoding="utf-8"
            )
            self._unsaved = 0
        except OSError:
            _log.warning(f"Could not write HLTB cache: {self._path}")


class SteamClient:
    """Fetches owned game lists from the Steam Web API. No business logic."""

    _MAX_ATTEMPTS = 3
    _API_URL = "https://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/"

    def __init__(self, api_key: str):
        self._api_key = api_key
        self._requests = get_requests()

    def get_owned_games(self, steam_id: str) -> Optional[Dict]:
        """Return the 'response' dict from Steam API, or None on error."""
        params = {
            "key": self._api_key,
            "steamid": steam_id,
            "format": "json",
            "include_appinfo": 1,
        }
        for attempt in range(1, self._MAX_ATTEMPTS + 1):
            try:
                response = self._requests.get(self._API_URL, params=params, timeout=30)
                response.raise_for_status()
                return response.json().get('response', {})
            except self._requests.exceptions.RequestException as e:
                # Log the exception type only — its message can embed the full
                # request URL, which contains the API key.
                _log.warning(
                    f"Steam API error for {steam_id} "
                    f"(attempt {attempt}/{self._MAX_ATTEMPTS}): {type(e).__name__}"
                )
                if attempt < self._MAX_ATTEMPTS:
                    time.sleep(2 ** (attempt - 1))
            except ValueError:
                _log.error(f"Invalid JSON response for {steam_id}")
                return None
        return None


class HltbClient:
    """Wraps HowLongToBeat search. Returns raw hours, no formatting."""

    def __init__(self):
        self._hltb = get_howlongtobeat()()

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
    cache: Optional[HltbCache] = None,
) -> Dict[str, Optional[float]]:
    """
    Fetch completion hours for all unique games across the given Steam IDs.
    Network lookups are rate-limited to _HLTB_DELAY apart; cache hits cost
    nothing and never sleep.
    Returns {game_name: hours_or_None}.
    """
    results: Dict[str, Optional[float]] = {}
    did_network_lookup = False

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

            if cache is not None:
                hit, hours = cache.get(name)
                if hit:
                    results[name] = hours
                    continue

            if did_network_lookup:
                time.sleep(_HLTB_DELAY)
            hours = hltb_client.get_main_story_hours(name)
            did_network_lookup = True
            results[name] = hours
            if cache is not None:
                cache.set(name, hours)

            if hours:
                _log.info(f"+ {name}: {hours:.1f} hours")
            else:
                _log.warning(f"- {name}: no completion data")

    if cache is not None:
        cache.save()
    return results
