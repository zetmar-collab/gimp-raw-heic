# gimp-raw-heic

Wtyczka do **GIMP 3.2** (Windows 11 x64), która otwiera **pliki RAW z aparatów** oraz **zdjęcia HEIC/HEIF/AVIF z HDR** bezpośrednio w GIMP. Nie potrzebuje darktable, RawTherapee, dcraw ani żadnego innego programu: wywoływanie RAW robi biblioteka **LibRaw 0.22.2**, wkompilowana w plik wtyczki.

Wersja: **0.9.0** · Licencja: **GPL-3.0-or-later** · Szczegóły licencji bibliotek: [LICENSES.md](LICENSES.md)

---

## Instalacja

**Pobierz** plik `gimp-raw-heic-<wersja>-win64.zip` z zakładki [Releases](https://github.com/zetmar-collab/gimp-raw-heic/releases).

1. Zamknij GIMP.
2. Rozpakuj ZIP. Folder **`gimp-raw-heic`** skopiuj do:
   ```
   %APPDATA%\GIMP\3.2\plug-ins\
   ```
   Wynik: `%APPDATA%\GIMP\3.2\plug-ins\gimp-raw-heic\gimp-raw-heic.exe`.
   (Ścieżkę `%APPDATA%` wklej w pasek adresu Eksploratora; nie potrzeba uprawnień administratora.)
3. Uruchom GIMP. Pierwsze uruchomienie po instalacji trwa kilka sekund dłużej, bo GIMP rejestruje nową wtyczkę.

Wymagania: **GIMP 3.2 lub nowszy 3.x** w wersji instalowanej z gimp.org. Wtyczka korzysta z bibliotek, które GIMP już ma (libheif, lcms2, OpenMP, GEGL), więc nic więcej nie trzeba instalować. Zbudowano i przetestowano na GIMP 3.2.6. Na GIMP 3.0.x wtyczki nie testowano i to nie jest wspierana konfiguracja.

### Czy GIMP używa tej wtyczki do RAW?

GIMP używa naraz tylko jednego importera RAW. Wtyczka przejmuje tę rolę automatycznie, o ile wcześniej nie wybrano darktable lub RawTherapee. Sprawdzisz to w **Edycja → Preferencje → Import i eksport obrazów → Importer RAW**: powinno tam być `gimp-raw-heic`.

## Obsługiwane formaty

**RAW (przez LibRaw, ok. 1250 modeli aparatów):** CR2, CR3, CRW (Canon), NEF, NRW (Nikon), ARW, SRF, SR2 (Sony), DNG (także z telefonów, Apple ProRAW, linear DNG), RAF (Fujifilm, także X-Trans), ORF (Olympus / OM System), RW2 (Panasonic), RWL (Leica), PEF (Pentax), SRW (Samsung), 3FR, FFF (Hasselblad), IIQ, CAP (Phase One), ERF (Epson), MRW, MDC (Minolta), KDC, DCR (Kodak), MEF (Mamiya), MOS (Leaf), X3F (Sigma), GPR (GoPro), RAW, RWZ i inne, które otwiera LibRaw.

**HEIC / HEIF / AVIF:** pliki z **HDR** (krzywe PQ i HLG) wtyczka dekoduje sama. Wszystkie pozostałe (zwykłe zdjęcia z iPhone'a, 8/10/12-bit, z kanałem alfa) przekazuje do **wbudowanego w GIMP modułu HEIF**, który robi to dobrze. Wtyczka nie dubluje jego kodu, tylko dodaje to, czego mu brakuje: HDR i szybkie miniatury w oknie Otwórz.

## Jak używać

### Otwieranie RAW
*Plik → Otwórz*, przeciągnięcie pliku na okno albo *Otwórz jako warstwy*. Pokaże się okno **Wywołaj RAW** z podglądem na żywo (połowa rozdzielczości, odświeża się ok. 0,5 s po zmianie):

| Zakładka | Ustawienia |
|---|---|
| **Balans bieli** | z aparatu · automatyczny · ręczny (temperatura K + tint) · **punkt neutralny**: kliknij w podglądzie szary obszar |
| **Ekspozycja** | ekspozycja −2…+3 EV, ochrona świateł, odzyskiwanie prześwietleń (obetnij / nie obcinaj / mieszaj / odbuduj 3–9), jasność, automatyczna jasność |
| **Szczegóły** | demozaikowanie (dwuliniowe, VNG, PPG, **AHD**, DCB, DHT, AAHD; matryce X-Trans zawsze dostają algorytm X-Trans), redukcja szumu falkowa i FBDD, korekta aberracji chromatycznej (R/B), usuwanie gorących i martwych pikseli, filtr medianowy artefaktów barwnych |
| **Wyjście** | przestrzeń barw: sRGB, Adobe RGB, Display P3, ProPhoto, Rec. 2020 · światło liniowe lub z krzywą tonalną · precyzja 8/16/32-bit float · połowa rozmiaru · obrót wg aparatu · domyślne kadrowanie aparatu |

- Ustawienia są **zapamiętywane** między sesjami.
- **„Zawsze pokazuj to okno”** wyłączone: pliki otwierają się od razu z ostatnimi ustawieniami. **Shift** przytrzymany przy otwieraniu odwraca to zachowanie (pokazuje okno albo je pomija).
- Domyślnie powstaje obraz **16-bit liniowy** w sRGB z poprawnym profilem ICC. Krzywe, poziomy i maski działają bez posteryzacji.
- **EXIF, XMP i IPTC** z pliku RAW trafiają do obrazu i są zapisywane przy eksporcie. Orientacja jest stosowana do pikseli, a znacznik w EXIF ustawiany na „normalna”, żeby nic nie obracało zdjęcia drugi raz.

### Ponowne wywołanie RAW
**Filtry → RAW → Wywołaj RAW ponownie…**

Obraz pamięta ścieżkę oryginalnego pliku RAW i użyte ustawienia. Zapisuje je w pliku XCF, więc działa to także po zamknięciu i ponownym otwarciu XCF. Okno startuje z ustawieniami użytymi dla tego obrazu. Wynik **zastępuje wywołaną warstwę** albo jest **dodawany jako nowa warstwa** (np. dwie ekspozycje do maskowania nieba). Oryginalny RAW musi leżeć w tym samym miejscu na dysku.

### Szybka korekta nieniszcząca
**Filtry → RAW → Szybka korekta (nieniszcząca)…**

Dodaje do warstwy **edytowalne efekty GEGL**: ekspozycję i poziom czerni, temperaturę barwową (z/do), cienie i światła, kontrast, nasycenie oraz opcjonalnie neutralne **Poziomy** i **Krzywe** do późniejszego dopracowania. Efekty widać w doku Warstwy (ikona efektów). Można je zmieniać, wyłączać i usuwać, a zapisują się w XCF.

### Wiele plików naraz
**Plik → Otwórz → Otwórz wiele plików RAW…**

Wybierasz pliki, ustawiasz wywołanie raz (z podglądem pierwszego pliku) i wtyczka otwiera kolejno wszystkie, z paskiem postępu. Anulowanie zatrzymuje kolejkę przed następnym plikiem. Pliki, których nie da się otworzyć, są wymienione na końcu.

### HEIC z HDR
Zdjęcia HDR (PQ / HLG, np. z nowych telefonów i kamer) domyślnie trafiają do GIMP jako **32-bit float liniowy w przestrzeni Rec. 2020 (BT.2100)**. Biel odniesienia SDR (203 cd/m²) ma wartość 1,0, a jaśniejsze partie mają wartości powyżej 1,0 i nie są tracone. Na zwykłym monitorze najjaśniejsze miejsca wyglądają na przepalone, dopóki nie zastosujesz np. *Kolory → Ekspozycja* albo mapowania tonów.

Przetestowano na zdjęciach z **realme 14 Pro 5G** (HEIC 4096×2304 złożony z 40 kafelków, profil Display P3, EXIF). Otwierają się w ok. 0,5 s, z zachowanym profilem Display P3 i metadanymi. Te pliki nie zawierają żadnej miniatury (ani w HEIF, ani w EXIF), więc miniatura w oknie Otwórz powstaje z pełnego obrazu (ok. 0,5 s, tylko za pierwszym razem, bo potem GIMP trzyma ją w pamięci podręcznej). Jeśli telefon zapisuje miniaturę w HEIF lub EXIF, wtyczka używa jej od razu.

Inne tryby wybierzesz w procedurze `file-raw-heic-heif-load`, parametr *Obrazy HDR*:
- **Mapowanie tonów do SDR** (16-bit sRGB): od razu dobrze wygląda, ale traci zakres HDR.
- **Wbudowany moduł GIMP dla wszystkich plików**: całkowicie wyłącza HEIC w tej wtyczce.

### Informacje
*Pomoc → O wtyczce RAW / HEIC* pokazuje wersje wtyczki, LibRaw i libheif.

## Ograniczenia (stan na 0.9.0)

- **Odzyskiwanie prześwietleń** w trybach innych niż „Obetnij” przyciemnia obraz o ok. 1 EV. LibRaw zostawia w ten sposób miejsce na odzyskane światła. Skompensuj suwakiem *Ekspozycja* albo później w GIMP.
- **Redukcja szumu falkowa** to własna implementacja wtyczki, działająca na wywołanym obrazie. Odpowiednik w LibRaw 0.22.2 (wersja z MSYS2) wywraca program, także jego własne narzędzie `dcraw_emu`.
- **Algorytmy demozaikowania z pakietów GPL2/GPL3** (AFD, VCD, AMaZE, LMMSE…) nie istnieją w LibRaw od wersji 0.20, więc nie są dostępne. Dostępne algorytmy wymienia tabela wyżej.
- **Połowa rozmiaru** nie zmniejsza „linear DNG” (np. Apple ProRAW), bo nie mają mozaiki Bayera.
- **HDR z mapą wzmocnienia** (Apple „HDR photo”, Ultra HDR) nie jest odtwarzane. Takie zdjęcia otwierają się jako zwykłe SDR przez moduł GIMP.
- **Przy ponownym wywołaniu** precyzja obrazu się nie zmienia, a dane trafiają do jego bieżącego profilu barw. Ustawienia „Precyzja” i „Światło liniowe” działają tylko przy otwieraniu pliku.
- **8-bit** zawsze używa krzywej tonalnej, bo 8-bit liniowe powoduje posteryzację.
- **Uszkodzone pliki** nie wywracają GIMP-a (sprawdzone na 50 uszkodzonych próbkach). Część uciętych plików LibRaw jednak otwiera, uzupełniając brakującą część obrazu.
- **Skrypty** wywołujące `gimp_file_load` dostają ustawienia domyślne. Ustawienia przekażesz, wywołując procedurę `file-raw-heic-raw-load` bezpośrednio.
- **Linux** nie był budowany ani testowany (kod nie zawiera nic specyficznego dla Windows poza wykrywaniem klawisza Shift).

## Odinstalowanie

Usuń folder `%APPDATA%\GIMP\3.2\plug-ins\gimp-raw-heic`. Jeśli w *Preferencje → Import i eksport obrazów → Importer RAW* była wybrana ta wtyczka, GIMP przy następnym starcie wróci do swojego ustawienia domyślnego.

## Prywatność

Wtyczka nie łączy się z siecią i nie wysyła żadnych danych.

---

## Dla programistów

### Budowanie (Windows, MSYS2 CLANG64, czyli tym samym toolchainem co GIMP)
```powershell
.\build.ps1 -Setup      # jednorazowo: pakiety MSYS2 (clang, meson, gimp, libraw, libheif, …)
.\build.ps1 -Install    # kompilacja + instalacja do %APPDATA%\GIMP\3.x\plug-ins
.\build.ps1 -Package    # dist\gimp-raw-heic-<wersja>-win64.zip + paczka źródeł
.\build.ps1 -UpdatePo   # odśwież po/gimp-raw-heic.pot i po/pl.po
```

### Testy
```powershell
sh tests/fetch-samples.sh                 # próbki RAW z raw.pixls.us (CC0)
python tests/make_heic_samples.py         # próbki HEIC (SDR, 10-bit, alfa, PQ, HLG)
.\tests\run-tests.ps1                     # wszystkie testy w gimp-console
```
Testy: ładowanie wszystkich formatów, porównanie z `dcraw_emu` (referencja LibRaw; najpierw wygeneruj `build\ref\*.tiff` poleceniem `dcraw_emu -w -4 -T -o 1 -q 3 <plik>`), miniatury i uszkodzone pliki, HEIC (wartości PQ/HLG), ponowne wywołanie z XCF i filtry nieniszczące, metadane, macierz opcji.

### Struktura
`src/plugin.c` rejestracja procedur · `raw-load.c` loader i miniatury RAW · `raw-develop.c` LibRaw (balans bieli z temperatury, punkt neutralny, gorące piksele, limity bezpieczeństwa) · `denoise.c` redukcja szumu · `raw-dialog.c` okno z podglądem · `image.c` tworzenie obrazu i profile · `profiles.c` profile ICC · `redevelop.c` ponowne wywołanie · `adjust.c` filtry nieniszczące · `batch.c` wiele plików · `heif-load.c` HEIC HDR · `settings.c` ustawienia i ich zapis · `po/` tłumaczenie.
