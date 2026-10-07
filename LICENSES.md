# Licencje

## gimp-raw-heic

Copyright © 2026 Marek Zettel.
Program na licencji **GNU General Public License w wersji 3 lub nowszej** (pełny tekst: [COPYING](COPYING)). Kod źródłowy jest w paczce `gimp-raw-heic-<wersja>-source.zip` i w repozytorium projektu.

## Co zawiera paczka binarna

Plik `gimp-raw-heic.exe` zawiera **wkompilowaną statycznie** jedną bibliotekę zewnętrzną:

| Biblioteka | Wersja | Licencja | Uwagi |
|---|---|---|---|
| **LibRaw** | 0.22.2 | LGPL-2.1 **lub** CDDL-1.0 (do wyboru) | używana na warunkach **LGPL-2.1**, zgodnych z GPL-3.0; teksty w `licenses/LibRaw-LGPL-2.1.txt` i `licenses/LibRaw-CDDL-1.0.txt` |

Copyright LibRaw: © 2008–2025 LibRaw LLC (info@libraw.org); LibRaw wywodzi się z dcraw autorstwa Dave’a Coffina.

**Zgodność z GPL.** LGPL-2.1 pozwala łączyć bibliotekę z programem na GPL (§3 LGPL-2.1 dopuszcza nawet objęcie jej warunkami GPL). Przy linkowaniu statycznym LGPL wymaga, by użytkownik mógł zbudować program z inną wersją biblioteki. Ten warunek spełnia udostępnienie pełnego kodu źródłowego wtyczki (GPL) oraz skryptu `build.ps1` i `meson.build`. Kod źródłowy LibRaw 0.22.2 jest dostępny na https://www.libraw.org/ oraz w pakiecie źródłowym MSYS2 `mingw-w64-libraw`. **CDDL nie jest zgodna z GPL**, dlatego tej opcji licencyjnej nie wybieramy.

## Biblioteki używane z instalacji GIMP (nie są rozpowszechniane z wtyczką)

Wtyczka ładuje w czasie działania biblioteki DLL, które są już częścią GIMP-a. Paczka ich nie zawiera, więc obowiązki licencyjne związane z ich dystrybucją spoczywają na dystrybutorze GIMP-a. Lista dla porządku:

| Biblioteka | Licencja | Do czego |
|---|---|---|
| libgimp, libgimpui (GIMP 3.2) | LGPL-3.0-or-later | API wtyczek |
| GEGL, babl | LGPL-3.0-or-later | bufory obrazu, konwersje kolorów, filtry |
| GLib, GTK 3 | LGPL-2.1-or-later | interfejs |
| libheif | LGPL-3.0 | dekodowanie HEIF/HEIC/AVIF |
| libde265 | LGPL-3.0 | dekoder HEVC (HEIC) |
| dav1d, libaom | BSD-2-Clause / BSD-2-Clause + patent AOM | dekoder AV1 (AVIF) |
| Little CMS 2 | MIT | profile ICC |
| GExiv2 / Exiv2 | GPL-2.0-or-later | metadane |
| libjpeg-turbo, zlib | IJG/BSD, zlib | używane przez LibRaw |
| LLVM OpenMP, libc++ | Apache-2.0 WITH LLVM-exception | wielowątkowość, C++ |

Wszystkie te licencje są zgodne z GPL-3.0.

## Patenty HEVC (informacyjnie)

HEIC zawiera obraz zakodowany w **HEVC (H.265)**, którego dotyczą patenty kilku grup licencyjnych (np. Access Advance, Via LA). Dekoder libde265 jest oprogramowaniem open source, ale licencja oprogramowania nie obejmuje licencji patentowych. W niektórych krajach komercyjna dystrybucja dekodera HEVC może wymagać licencji patentowej. Wtyczka nie zawiera własnego dekodera HEVC (używa libde265 dostarczonego z GIMP) i nie koduje HEVC. AV1 (AVIF) jest udostępniany przez Alliance for Open Media z bezpłatną licencją patentową. To informacja, a nie porada prawna.
