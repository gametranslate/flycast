# Clarity UI CJK

These subsets of Noto Sans CJK 2.004 cover the non-ASCII characters in
`strings.json` and the language selector. Three faces preserve Simplified
Chinese, Traditional Chinese and Korean forms, in that order. Regular and
Bold are embedded only in Clarity standalone builds. Roboto remains the Latin
font; system fonts provide additional characters for game and device names.

Upstream: https://github.com/notofonts/noto-cjk (SIL OFL 1.1, see `OFL.txt`).
The subsets are renamed Clarity UI CJK and retain upstream copyright metadata.
The full license is also embedded and viewable in About on every platform.

To regenerate after adding translations, install `fonttools==4.60.1` and run
`generate.py` with NotoSansCJK-Regular.ttc and NotoSansCJK-Bold.ttc from
Ubuntu 24.04's `fonts-noto-cjk`. The script verifies both source SHA-256 hashes.
Commit the generated ZIP files. Building Flycast does not require fonttools.
The native smoke suite checks every translated character in both font weights
for each of the eight selected locales, as well as all language-selector names.
