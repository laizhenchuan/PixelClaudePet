"""Get current Windows media playback info + lyrics lookup."""
import asyncio
import urllib.request
import json

try:
    from pypinyin import lazy_pinyin
    HAS_PINYIN = True
except ImportError:
    HAS_PINYIN = False

def _to_pinyin(text):
    """Convert Chinese text to pinyin. Non-Chinese chars pass through."""
    if not HAS_PINYIN or not text:
        return text
    result = []
    for ch in text:
        if '一' <= ch <= '鿿':
            py = lazy_pinyin(ch)
            result.append(py[0] if py else ch)
        else:
            result.append(ch)
    return ''.join(result)

_cache_title = ''
_cache_lyrics = ''

async def _get_media_info():
    try:
        from winrt.windows.media.control import (
            GlobalSystemMediaTransportControlsSessionManager as MediaManager
        )
        sessions = await MediaManager.request_async()
        session = sessions.get_current_session()
        if session:
            info = await session.try_get_media_properties_async()
            return {
                'title': info.title or '',
                'artist': info.artist or '',
            }
    except Exception:
        pass
    return {'title': '', 'artist': ''}

def get_media_info():
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
    try:
        return loop.run_until_complete(_get_media_info())
    except Exception:
        return {'title': '', 'artist': ''}

def get_lyrics(title, artist):
    """Fetch lyrics from lyrics.ovh API. Returns lyrics text or empty string."""
    global _cache_title, _cache_lyrics
    if not title:
        return ''
    if title == _cache_title:
        return _cache_lyrics
    try:
        a = artist or ''
        url = f'https://api.lyrics.ovh/v1/{urllib.parse.quote(a)}/{urllib.parse.quote(title)}'
        req = urllib.request.Request(url, headers={'User-Agent': 'PixelClaudePet/1.0'})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read())
            lyrics = data.get('lyrics', '')
            lyrics = _to_pinyin(lyrics)  # Convert Chinese to pinyin
            _cache_title = title
            _cache_lyrics = lyrics
            return lyrics
    except Exception:
        pass
    return ''
