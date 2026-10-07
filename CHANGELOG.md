# Zmiany

## 0.9.0 – 2026-10-07

Pierwsza wersja.

### RAW
- Wczytywanie RAW przez LibRaw 0.22.2 (wkompilowany statycznie): CR2, CR3, CRW, NEF, NRW, ARW, SRF, SR2, DNG, RAF (X-Trans), ORF, RW2, RWL, PEF, SRW, 3FR, FFF, IIQ, ERF, MRW, KDC, DCR, MEF, MOS, X3F, GPR i inne.
- Okno „Wywołaj RAW” z podglądem na żywo (wątek w tle) i zapamiętywaniem ustawień; opcja „Zawsze pokazuj to okno” i odwracanie klawiszem Shift.
- Balans bieli z aparatu, automatyczny, ręczny (temperatura/tint liczone z macierzy aparatu) i punkt neutralny klikany w podglądzie.
- Ekspozycja, ochrona świateł, 4 tryby odzyskiwania prześwietleń, jasność, automatyczna jasność.
- Demozaikowanie: dwuliniowe, VNG, PPG, AHD, DCB, DHT, AAHD; X-Trans automatycznie.
- Redukcja szumu: falkowa (własna implementacja) i FBDD; aberracja chromatyczna; gorące/martwe piksele; mediana artefaktów barwnych.
- Wyjście: sRGB, Adobe RGB, Display P3, ProPhoto, Rec. 2020; liniowe lub z krzywą; 8/16/32-bit float; połowa rozmiaru; orientacja; domyślne kadrowanie aparatu; poprawne profile ICC.
- Metadane EXIF/XMP/IPTC przekazywane do GIMP i zachowywane przy eksporcie.
- Szybkie miniatury z osadzonego JPEG (0,1–0,2 s).
- Filtry → RAW → Wywołaj RAW ponownie (ustawienia i ścieżka w XCF; zastąpienie warstwy lub nowa warstwa).
- Filtry → RAW → Szybka korekta (nieniszcząca): edytowalne efekty GEGL.
- Plik → Otwórz → Otwórz wiele plików RAW (kolejka z paskiem postępu i anulowaniem).
- Limity bezpieczeństwa rozmiarów, odporność na uszkodzone pliki, niskie zużycie pamięci (ok. 16 B/piksel szczytowo).

### HEIC / HEIF / AVIF
- Dekodowanie HDR (PQ i HLG) do 32-bit float liniowego Rec. 2100 (biel odniesienia = 1,0) lub z mapowaniem tonów do SDR.
- Pliki SDR przekazywane do wbudowanego modułu HEIF w GIMP.
- Miniatury HEIF w oknie Otwórz: z miniatury HEIF, z miniatury JPEG w EXIF albo z pełnego obrazu.
- Sprawdzono na zdjęciach HEIC z realme 14 Pro 5G (kafelki, Display P3, EXIF).

### Inne
- Interfejs po polsku i angielsku (gettext).
- Skrypt budujący `build.ps1`, testy wsadowe w `gimp-console`.
