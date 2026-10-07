"""Regenerate po/pl.po from po/gimp-raw-heic.pot and the table below.

Run after `xgettext` (see build.ps1).  Untranslated strings are reported.
"""
import re
import sys

T = {
    'Quick adjustments (non-destructive)...': 'Szybka korekta (nieniszcząca)...',
    'Add editable exposure, white balance and tone filters': 'Dodaj edytowalne filtry ekspozycji, balansu bieli i tonów',
    'Adds GEGL filters as non-destructive layer effects: exposure, colour temperature, shadows/highlights, contrast, saturation and optionally empty Levels and Curves.  Each one stays editable later.': 'Dodaje filtry GEGL jako nieniszczące efekty warstwy: ekspozycję, temperaturę barwową, cienie/światła, kontrast, nasycenie oraz opcjonalnie neutralne Poziomy i Krzywe. Każdy z nich można później edytować.',
    'Exposure (EV)': 'Ekspozycja (EV)',
    'Exposure in stops': 'Ekspozycja w działkach (EV)',
    'Black level': 'Poziom czerni',
    'Black point offset': 'Przesunięcie punktu czerni',
    'Original temperature (K)': 'Temperatura oryginalna (K)',
    'Light the photo was taken in': 'Światło, w którym zrobiono zdjęcie',
    'Intended temperature (K)': 'Temperatura docelowa (K)',
    'Light it should look like': 'Światło, jakie ma przypominać',
    'Shadows': 'Cienie',
    'Lift (+) or deepen (-) shadows': 'Rozjaśnij (+) lub pogłęb (-) cienie',
    'Highlights': 'Światła',
    'Recover (-) or boost (+) highlights': 'Odzyskaj (-) lub wzmocnij (+) światła',
    'Contrast': 'Kontrast',
    'Contrast change': 'Zmiana kontrastu',
    'Saturation': 'Nasycenie',
    'Saturation scale, 1 = unchanged': 'Mnożnik nasycenia, 1 = bez zmian',
    'Add Levels for later editing': 'Dodaj Poziomy do późniejszej edycji',
    'Add an unchanged Levels effect': 'Dodaj neutralny efekt Poziomy',
    'Add Curves for later editing': 'Dodaj Krzywe do późniejszej edycji',
    'Add an unchanged Curves effect': 'Dodaj neutralny efekt Krzywe',
    'Select exactly one layer.': 'Zaznacz dokładnie jedną warstwę.',
    'Quick adjustments (non-destructive)': 'Szybka korekta (nieniszcząca)',
    'Exposure': 'Ekspozycja',
    'Colour temperature': 'Temperatura barwowa',
    'Shadows-Highlights': 'Cienie i światła',
    'Levels': 'Poziomy',
    'Curves': 'Krzywe',
    'All values are neutral: no effect was added.': 'Wszystkie wartości są neutralne – nie dodano żadnego efektu.',
    'Open several RAW files...': 'Otwórz wiele plików RAW...',
    'Develop several RAW files with the same settings': 'Wywołaj wiele plików RAW z tymi samymi ustawieniami',
    'Pick RAW files, choose development settings once and open them all; the progress can be cancelled.': 'Wybierz pliki RAW, ustaw parametry wywołania raz i otwórz wszystkie; postęp można anulować.',
    'Files': 'Pliki',
    'Paths of the RAW files to open': 'Ścieżki plików RAW do otwarcia',
    'Open several RAW files': 'Otwórz wiele plików RAW',
    '_Cancel': '_Anuluj',
    '_Open': '_Otwórz',
    'Camera RAW files': 'Pliki RAW z aparatu',
    'No files were given.': 'Nie podano żadnych plików.',
    'No file could be opened.\\n%s': 'Nie udało się otworzyć żadnego pliku.\\n%s',
    '(%d/%d) %s': '(%d/%d) %s',
    'Some files could not be opened:\\n%s': 'Niektórych plików nie udało się otworzyć:\\n%s',
    'Opening was cancelled.': 'Otwieranie zostało anulowane.',
    'Load the embedded thumbnail of a HEIF/AVIF file': 'Wczytaj osadzoną miniaturę pliku HEIF/AVIF',
    'Fast thumbnail from the preview stored in HEIF files.': 'Szybka miniatura z podglądu zapisanego w pliku HEIF.',
    'HEIF/HEIC/AVIF with HDR (PQ/HLG)': 'HEIF/HEIC/AVIF z HDR (PQ/HLG)',
    'Load HEIF/HEIC/AVIF images, including HDR': 'Wczytaj obrazy HEIF/HEIC/AVIF, także HDR',
    "Decodes HDR images (PQ and HLG transfer) into scene-linear floating point; all other images are loaded by GIMP's built-in HEIF loader.": 'Dekoduje obrazy HDR (krzywe PQ i HLG) do liniowych wartości zmiennoprzecinkowych; pozostałe obrazy wczytuje wbudowany w GIMP moduł HEIF.',
    '_HDR images': 'Obrazy _HDR',
    'What to do with HDR (PQ/HLG) images': 'Co zrobić z obrazami HDR (PQ/HLG)',
    'Scene-linear 32-bit float (keep HDR)': 'Liniowy 32-bit float (zachowaj HDR)',
    'Tone-map to SDR (16-bit sRGB)': 'Mapowanie tonów do SDR (16-bit sRGB)',
    "Use GIMP's built-in loader for all files": 'Użyj wbudowanego modułu GIMP dla wszystkich plików',
    "GIMP's built-in HEIF loader is not available.": 'Wbudowany w GIMP moduł HEIF jest niedostępny.',
    'HEIF files can only be opened from a local disk.': 'Pliki HEIF można otwierać tylko z dysku lokalnego.',
    "Could not read HEIF file '%s': %s": 'Nie można odczytać pliku HEIF „%s”: %s',
    'Decoding the HEIF image failed: %s': 'Dekodowanie obrazu HEIF nie powiodło się: %s',
    'The HEIF image has an unsupported layout.': 'Obraz HEIF ma nieobsługiwany układ danych.',
    'Not enough memory to load the image.': 'Za mało pamięci, aby wczytać obraz.',
    "Loading HDR image '%s'": 'Wczytywanie obrazu HDR „%s”',
    'Preview': 'Podgląd',
    'Could not create the image.': 'Nie można utworzyć obrazu.',
    'RAW': 'RAW',
    'About RAW / HEIC loader': 'O wtyczce RAW / HEIC',
    'Show gimp-raw-heic version': 'Pokaż wersję gimp-raw-heic',
    'Shows plug-in and library versions.': 'Pokazuje wersję wtyczki i bibliotek.',
    'gimp-raw-heic %s\\nLibRaw %s (%d cameras)\\nlibheif %s\\nLicense: GPL-3.0-or-later': 'gimp-raw-heic %s\\nLibRaw %s (%d aparatów)\\nlibheif %s\\nLicencja: GPL-3.0 lub nowsza',
    "Loading '%s' was cancelled.": 'Wczytywanie „%s” zostało anulowane.',
    "'%s' is not a supported RAW file or the camera is unknown to LibRaw %s.": '„%s” nie jest obsługiwanym plikiem RAW albo LibRaw %s nie zna tego aparatu.',
    "Not enough memory to load '%s'.": 'Za mało pamięci, aby wczytać „%s”.',
    "'%s' is damaged or truncated (%s).": 'Plik „%s” jest uszkodzony lub niekompletny (%s).',
    "Could not load '%s': %s": 'Nie można wczytać „%s”: %s',
    "'%s' reports an invalid image size (%u x %u).": 'Plik „%s” podaje nieprawidłowy rozmiar obrazu (%u × %u).',
    'Could not initialise LibRaw.': 'Nie można zainicjować LibRaw.',
    '%s — preview %d ms': '%s — podgląd %d ms',
    'Developing preview…': 'Wywoływanie podglądu…',
    '_Develop': '_Wywołaj',
    'White balance': 'Balans bieli',
    'Detail': 'Szczegóły',
    'Output': 'Wyjście',
    'Tip: click the preview to pick a neutral (grey) area.': 'Wskazówka: kliknij podgląd, aby wskazać neutralny (szary) obszar.',
    'Camera RAW (LibRaw)': 'RAW z aparatu (LibRaw)',
    'Load camera RAW files with LibRaw': 'Wczytaj pliki RAW z aparatu przez LibRaw',
    'Develops camera RAW files (CR2, CR3, NEF, ARW, RAF, DNG, RW2, ORF, PEF and many more) inside GIMP using LibRaw, without external programs.': 'Wywołuje pliki RAW (CR2, CR3, NEF, ARW, RAF, DNG, RW2, ORF, PEF i wiele innych) bezpośrednio w GIMP za pomocą LibRaw, bez programów zewnętrznych.',
    'Always show this dialog': 'Zawsze pokazuj to okno',
    'Show the import settings for every file (hold Shift while opening to invert)': 'Pokazuj ustawienia importu dla każdego pliku (Shift przy otwieraniu odwraca to zachowanie)',
    'Load the embedded preview of a RAW file': 'Wczytaj osadzony podgląd pliku RAW',
    'Fast thumbnail from the JPEG preview that cameras embed in RAW files.': 'Szybka miniatura z podglądu JPEG osadzanego przez aparaty w plikach RAW.',
    'RAW files can only be opened from a local disk.': 'Pliki RAW można otwierać tylko z dysku lokalnego.',
    'Develop RAW – %s': 'Wywołaj RAW – %s',
    "Developing '%s'": 'Wywoływanie „%s”',
    "Creating image from '%s'": 'Tworzenie obrazu z „%s”',
    'Develop RAW again...': 'Wywołaj RAW ponownie...',
    'Develop the original RAW file again with new settings': 'Wywołaj oryginalny plik RAW ponownie z nowymi ustawieniami',
    'Uses the RAW file path and settings stored in the image (also in XCF files) to develop the RAW again, replacing the developed layer or adding a new layer.': 'Używa ścieżki pliku RAW i ustawień zapisanych w obrazie (także w plikach XCF), aby wywołać RAW ponownie – zastępując wywołaną warstwę lub dodając nową.',
    'Result': 'Wynik',
    'Replace the developed layer or add a new one': 'Zastąp wywołaną warstwę lub dodaj nową',
    'Replace the developed layer': 'Zastąp wywołaną warstwę',
    'Add as a new layer': 'Dodaj jako nową warstwę',
    'This image was not loaded from a RAW file by gimp-raw-heic, so there is nothing to develop again.': 'Ten obraz nie został wczytany z pliku RAW przez gimp-raw-heic, więc nie ma czego wywoływać ponownie.',
    'The RAW information stored in this image is damaged.': 'Informacje o RAW zapisane w tym obrazie są uszkodzone.',
    'The original RAW file was not found:\\n%s\\n\\nPut it back at this location to develop it again.': 'Nie znaleziono oryginalnego pliku RAW:\\n%s\\n\\nPrzywróć go w to miejsce, aby wywołać go ponownie.',
    'Develop RAW again – %s': 'Wywołaj RAW ponownie – %s',
    'Developing RAW again': 'Ponowne wywoływanie RAW',
    '%s (developed again)': '%s (wywołany ponownie)',
    'As shot (camera)': 'Z aparatu',
    'Automatic': 'Automatyczny',
    'Manual (temperature / tint)': 'Ręczny (temperatura / tint)',
    'Neutral point (picked area)': 'Punkt neutralny (wskazany obszar)',
    'Clip': 'Obetnij',
    'Unclip': 'Nie obcinaj',
    'Blend': 'Mieszaj',
    'Rebuild': 'Odbuduj',
    'Bilinear (fastest)': 'Dwuliniowy (najszybszy)',
    'Off': 'Wyłączona',
    'Light': 'Lekka',
    'Full': 'Pełna',
    '8-bit integer': '8-bit całkowite',
    '16-bit integer': '16-bit całkowite',
    '32-bit floating point': '32-bit zmiennoprzecinkowe',
    'White balance mode': 'Tryb balansu bieli',
    'Temperature (K)': 'Temperatura (K)',
    'Colour temperature for manual white balance': 'Temperatura barwowa dla ręcznego balansu bieli',
    'Tint': 'Tint',
    'Tint for manual white balance: negative = green, positive = magenta': 'Tint dla ręcznego balansu bieli: ujemny = zieleń, dodatni = magenta',
    'Spot X': 'Punkt X',
    'Neutral area left edge (0..1)': 'Lewa krawędź obszaru neutralnego (0..1)',
    'Spot Y': 'Punkt Y',
    'Neutral area top edge (0..1)': 'Górna krawędź obszaru neutralnego (0..1)',
    'Spot width': 'Szerokość punktu',
    'Neutral area width (0..1)': 'Szerokość obszaru neutralnego (0..1)',
    'Spot height': 'Wysokość punktu',
    'Neutral area height (0..1)': 'Wysokość obszaru neutralnego (0..1)',
    'Exposure correction applied before demosaicing': 'Korekta ekspozycji stosowana przed demozaikowaniem',
    'Preserve highlights': 'Chroń światła',
    'Highlight preservation for positive exposure (0..1)': 'Ochrona świateł przy dodatniej ekspozycji (0..1)',
    'Highlight recovery': 'Odzyskiwanie prześwietleń',
    'How clipped highlights are handled; modes other than Clip darken the image by about 1 EV to make room for the recovered highlights (compensate with Exposure)': 'Sposób traktowania prześwietleń; tryby inne niż „Obetnij” przyciemniają obraz o ok. 1 EV, aby zmieścić odzyskane światła (skompensuj Ekspozycją)',
    'Rebuild level': 'Poziom odbudowy',
    "Strength of the 'rebuild' highlight mode": 'Siła trybu odbudowy prześwietleń',
    'Brightness': 'Jasność',
    'Output brightness multiplier': 'Mnożnik jasności wyjściowej',
    'Automatic brightness': 'Automatyczna jasność',
    'Stretch the histogram like dcraw (clips one percent of pixels)': 'Rozciągnij histogram jak dcraw (obcina jeden procent pikseli)',
    'Demosaicing': 'Demozaikowanie',
    'Interpolation algorithm (X-Trans sensors always use the X-Trans algorithm)': 'Algorytm interpolacji (matryce X-Trans zawsze używają algorytmu X-Trans)',
    'Noise reduction (wavelet)': 'Redukcja szumu (falkowa)',
    'Wavelet denoising threshold, 0 = off (typical 100..1000)': 'Próg falkowej redukcji szumu, 0 = wyłączona (typowo 100..1000)',
    'FBDD noise reduction': 'Redukcja szumu FBDD',
    'FBDD noise reduction before demosaicing': 'Redukcja szumu FBDD przed demozaikowaniem',
    'Chromatic aberration – red': 'Aberracja chromatyczna – czerwony',
    'Red channel scale, 1.0 = off': 'Skala kanału czerwonego, 1,0 = wyłączone',
    'Chromatic aberration – blue': 'Aberracja chromatyczna – niebieski',
    'Blue channel scale, 1.0 = off': 'Skala kanału niebieskiego, 1,0 = wyłączone',
    'Remove hot / dead pixels': 'Usuń gorące / martwe piksele',
    'Replace isolated outlier sensor pixels': 'Zastąp pojedyncze odstające piksele matrycy',
    'Colour artefact median passes': 'Przebiegi mediany artefaktów barwnych',
    '3x3 median filter passes on colour differences': 'Liczba przebiegów filtru medianowego 3×3 na różnicach kolorów',
    'Output colour space': 'Wyjściowa przestrzeń barw',
    'Colour space of the loaded image': 'Przestrzeń barw wczytanego obrazu',
    'Linear light (no gamma)': 'Światło liniowe (bez gammy)',
    "Keep scene-linear data; otherwise use the colour space's tone curve": 'Zachowaj dane liniowe; w przeciwnym razie użyj krzywej tonalnej przestrzeni barw',
    'Precision': 'Precyzja',
    'Bit depth of the loaded image': 'Głębia bitowa wczytanego obrazu',
    'Half size (fast)': 'Połowa rozmiaru (szybko)',
    'Load at half resolution': 'Wczytaj w połowie rozdzielczości',
    'Rotate according to camera orientation': 'Obróć zgodnie z orientacją z aparatu',
    'Apply the orientation stored by the camera': 'Zastosuj orientację zapisaną przez aparat',
    'Apply camera default crop': 'Zastosuj domyślne kadrowanie aparatu',
    'Use the default crop stored in the file (DNG, CR3, ...)': 'Użyj domyślnego kadru zapisanego w pliku (DNG, CR3, ...)',
}

PLURAL = {
    'Develop %d RAW file': ('Wywołaj %d plik RAW', 'Wywołaj %d pliki RAW', 'Wywołaj %d plików RAW'),
}

# names that stay as they are
SAME = {'VNG', 'PPG', 'AHD', 'DCB', 'DHT', 'AAHD', 'sRGB', 'Adobe RGB (1998)',
        'Display P3', 'ProPhoto RGB', 'Rec. 2020', 'RAW', '(%d/%d) %s'}

HEADER = r'''# Polish translation for gimp-raw-heic.
# This file is distributed under the same license as gimp-raw-heic.
msgid ""
msgstr ""
"Project-Id-Version: gimp-raw-heic\n"
"Language: pl\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);\n"
'''


def esc(t):
    return t.replace('"', '\\"')


def main(pot_path="po/gimp-raw-heic.pot", out_path="po/pl.po"):
    pot = open(pot_path, encoding="utf-8").read()
    out, missing = [HEADER], []
    for block in pot.split("\n\n")[1:]:
        refs = "\n".join(l for l in block.splitlines() if l.startswith("#"))
        m = re.search(r'msgid ((?:".*"\n?)+)', block)
        mid = "".join(re.findall(r'"(.*)"', m.group(1))).replace('\\"', '"')
        plm = re.search(r'msgid_plural "(.*)"', block)
        if plm:
            forms = PLURAL.get(mid)
            if not forms:
                missing.append(mid)
                forms = ("", "", "")
            entry = '%s\nmsgid "%s"\nmsgid_plural "%s"\n' % (refs, esc(mid), plm.group(1))
            entry += "".join('msgstr[%d] "%s"\n' % (i, esc(f)) for i, f in enumerate(forms))
        else:
            tr = T.get(mid, mid if mid in SAME else "")
            if not tr:
                missing.append(mid)
            entry = '%s\nmsgid "%s"\nmsgstr "%s"\n' % (refs, esc(mid), esc(tr))
        out.append(entry)
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    for m in missing:
        print("UNTRANSLATED:", m)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
