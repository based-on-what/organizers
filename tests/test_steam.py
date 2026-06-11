"""Tests for analyzers.steam orchestration with stub clients. No network."""
import pytest

import analyzers.steam as steam_mod
from analyzers.steam import HltbCache, analyze_libraries


class StubSteamClient:
    def __init__(self, libraries):
        self._libraries = libraries

    def get_owned_games(self, steam_id):
        return self._libraries.get(steam_id)


class StubHltbClient:
    def __init__(self, hours_by_name):
        self._hours = hours_by_name
        self.lookups = []

    def get_main_story_hours(self, game_name):
        self.lookups.append(game_name)
        return self._hours.get(game_name)


@pytest.fixture(autouse=True)
def no_sleep(monkeypatch):
    monkeypatch.setattr(steam_mod.time, "sleep", lambda s: None)


def _games(*names):
    return {'games': [{'name': n} for n in names]}


class TestAnalyzeLibraries:
    def test_collects_hours(self):
        steam = StubSteamClient({'id1': _games('Portal', 'Hades')})
        hltb = StubHltbClient({'Portal': 3.0, 'Hades': 20.5})
        result = analyze_libraries(steam, hltb, ['id1'])
        assert result == {'Portal': 3.0, 'Hades': 20.5}

    def test_dedup_across_libraries(self):
        steam = StubSteamClient({
            'id1': _games('Portal', 'Hades'),
            'id2': _games('Portal', 'Celeste'),
        })
        hltb = StubHltbClient({'Portal': 3.0, 'Hades': 20.5, 'Celeste': 8.0})
        result = analyze_libraries(steam, hltb, ['id1', 'id2'])
        assert result == {'Portal': 3.0, 'Hades': 20.5, 'Celeste': 8.0}
        assert hltb.lookups.count('Portal') == 1

    def test_none_hours_kept(self):
        steam = StubSteamClient({'id1': _games('Obscure Game')})
        hltb = StubHltbClient({})
        result = analyze_libraries(steam, hltb, ['id1'])
        assert result == {'Obscure Game': None}

    def test_failed_library_skipped(self):
        steam = StubSteamClient({'id1': None, 'id2': _games('Portal')})
        hltb = StubHltbClient({'Portal': 3.0})
        result = analyze_libraries(steam, hltb, ['id1', 'id2'])
        assert result == {'Portal': 3.0}

    def test_empty_response_skipped(self):
        steam = StubSteamClient({'id1': {}})
        hltb = StubHltbClient({})
        assert analyze_libraries(steam, hltb, ['id1']) == {}


class TestHltbCache:
    def test_roundtrip(self, tmp_path):
        path = tmp_path / "cache.json"
        cache = HltbCache(path)
        cache.set("Portal", 3.0)
        cache.save()
        reloaded = HltbCache(path)
        assert reloaded.get("Portal") == (True, 3.0)

    def test_miss(self, tmp_path):
        cache = HltbCache(tmp_path / "cache.json")
        assert cache.get("Unknown") == (False, None)

    def test_none_hours_cached(self, tmp_path):
        cache = HltbCache(tmp_path / "cache.json")
        cache.set("Obscure", None)
        assert cache.get("Obscure") == (True, None)

    def test_expired_entry_is_miss(self, tmp_path):
        path = tmp_path / "cache.json"
        cache = HltbCache(path, ttl_seconds=0)
        cache.set("Portal", 3.0)
        # ttl 0: any entry is immediately stale
        assert cache.get("Portal") == (False, None)

    def test_corrupt_file_ignored(self, tmp_path):
        path = tmp_path / "cache.json"
        path.write_text("{ not json", encoding="utf-8")
        cache = HltbCache(path)
        assert cache.get("Portal") == (False, None)

    def test_cache_hits_skip_lookup(self, tmp_path):
        path = tmp_path / "cache.json"
        warm = HltbCache(path)
        warm.set("Portal", 3.0)
        warm.save()

        steam = StubSteamClient({'id1': _games('Portal', 'Hades')})
        hltb = StubHltbClient({'Portal': 3.0, 'Hades': 20.5})
        result = analyze_libraries(steam, hltb, ['id1'], cache=HltbCache(path))
        assert result == {'Portal': 3.0, 'Hades': 20.5}
        assert hltb.lookups == ['Hades']  # Portal served from cache

    def test_results_persisted_for_resume(self, tmp_path):
        path = tmp_path / "cache.json"
        steam = StubSteamClient({'id1': _games('Portal')})
        hltb = StubHltbClient({'Portal': 3.0})
        analyze_libraries(steam, hltb, ['id1'], cache=HltbCache(path))
        # second run: everything from cache, zero lookups
        hltb2 = StubHltbClient({'Portal': 3.0})
        result = analyze_libraries(steam, hltb2, ['id1'], cache=HltbCache(path))
        assert result == {'Portal': 3.0}
        assert hltb2.lookups == []
