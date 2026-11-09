"""Build embedded UI subsets from Noto Sans CJK 2.004 (fonttools 4.60.1).

Usage: python generate.py /path/to/NotoSansCJK-Regular.ttc /path/to/NotoSansCJK-Bold.ttc
The original TTC files are supplied by fonts-noto-cjk on Ubuntu 24.04.
"""
from pathlib import Path
from io import BytesIO
import hashlib
import json
import sys
import zipfile
from fontTools import subset
from fontTools.ttLib import TTCollection, TTFont

root = Path(__file__).resolve().parent
assert len(sys.argv) == 3, 'Supply Regular and Bold upstream TTC paths'
strings = json.loads((root.parent / 'strings.json').read_text(encoding='utf-8'))
text = ''.join(value for row in strings.values() for value in row)
text += 'EnglishTiếng ViệtDeutschFrançaisEspañol简体中文繁體中文한국어'
codepoints = sorted({ord(c) for c in text if ord(c) >= 128})
expected = ['b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a',
            'faa5f3656a78b2e2d450d27fe8382c778bc2b6bb5ea29c986664a6a435056ceb']
for source, weight, digest in zip(sys.argv[1:], ('Regular', 'Bold'), expected):
    raw = Path(source).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == digest, 'Unexpected upstream font version'
    collection = TTCollection()
    for index, region in ((2, 'SC'), (3, 'TC'), (1, 'KR')):
        font = TTFont(BytesIO(raw), fontNumber=index, recalcTimestamp=False)
        options = subset.Options()
        options.name_IDs = ['*']
        options.name_languages = ['*']
        options.name_legacy = True
        worker = subset.Subsetter(options=options)
        worker.populate(unicodes=codepoints)
        worker.subset(font)
        family = 'Clarity UI CJK ' + region
        for record in font['name'].names:
            names = {1: family, 2: weight, 3: family + '-' + weight,
                     4: family + ' ' + weight, 6: family.replace(' ', '') + '-' + weight,
                     16: family, 17: weight}
            if record.nameID in names:
                record.string = names[record.nameID].encode(record.getEncoding())
        if 'CFF ' in font:
            font['CFF '].cff.fontNames = [family.replace(' ', '') + '-' + weight]
            top = font['CFF '].cff.topDictIndex[0]
            top.FamilyName, top.FullName = family, family + ' ' + weight
        collection.fonts.append(font)
    buffer = BytesIO()
    collection.save(buffer)
    name = 'ClarityUI-' + weight + '.ttc'
    with zipfile.ZipFile(root / (name + '.zip'), 'w') as archive:
        entry = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
        entry.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(entry, buffer.getvalue(), compresslevel=9)
    print(name, len(buffer.getvalue()), 'bytes;', len(codepoints), 'requested characters')
