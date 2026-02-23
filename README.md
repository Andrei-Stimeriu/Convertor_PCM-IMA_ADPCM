Convertor fișiere wav: PCM <-> IMA ADPCM (C++ (consolă))
(Reverse engineering pe un MP3-Player chinezesc vechi)
(Codificare - decodificare IMA ADPCM)

COMENTARIILE DIN CODUL SURSĂ EXPLICĂ ÎN DETALIU RAȚIONAMENTUL DIN SPATELE ALGORITMULUI.

Se introduce calea fișierului inițial, apoi formatul final și calea fișierului final.

Acceptă fișiere wav în formatul PCM (8 / 16 / 24 / 32 biți per eșantion și oricâte canale) sau fișiere wav IMA ADPCM pe 4 biți (mono).
Convertește la PCM pe 16 biți (mono) sau la IMA ADPCM pe 4 biți (mono).
!! DE FĂCUT: Interpolare (momentan, fișierele de intrare PCM trebuie să aibă rata de eșantionare de 8000 Hz pentru a fi convertite la IMA ADPCM, deoarece aceasta e și rata fișierelor create de acel MP3-Player. Trebuie făcut algoritmul de conversie de la o rată de eșantionare la alta)

Înainte de convertire, se afișează informații despre fișier (tipul formatului, rata de eșantionare, durata, numărul de canale și alte detalii tehnice ce țin de fișierele wav).

Fișierele IMA ADPCM generate de program sunt după modelul celor create de un MP3-Player chinezesc vechi, cu funcție de înregistrare sonoră. (acele fișiere nu respectă întocmai specificația formatului, dar sunt redate corect de orice program. Vezi codul sursă pentru detalii)
