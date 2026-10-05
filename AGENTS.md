# Bmax: istruzioni per gli agenti AI

## Repository e documentazione

- Questo repository contiene il fork Bmax di Blender 5.2.2. Il branch di riferimento è `main`; il remoto di pubblicazione è `origin` (`https://github.com/MaxPuliero/Bmax.git`). Non pubblicare modifiche nel repository originale di Blender.
- Leggere `README.md`, `doc/bmax/FEATURES.md` e `doc/bmax/BUILD_WINDOWS.md` prima di modificare funzionalità o compilazione. L'inventario delle feature descrive lo stato implementato; `doc/armature_ui_plan.md` è un documento storico.
- Aggiornare README e inventario quando cambia il comportamento per l'utente. Aggiornare la guida di build quando cambiano i comandi, i percorsi o le verifiche. Non presentare proposte come funzionalità implementate.
- Il filtro delle linee di relazione è stato tolto dalle attività richieste: non reintrodurlo senza una nuova richiesta.
- I branch ritirati sono conservati nei tag `archive/armature-ui-2026-10-05` e `archive/bmax-publication-2026-10-05`. Non usarli come sorgente della build corrente.

## Test e spazio su disco

- Non creare o conservare file di test nel repository, salvo richiesta esplicita dell'utente.
- Per le verifiche necessarie, usare una directory temporanea esterna al repository; al termine eliminare gli script e gli artefatti creati per quelle verifiche (log, catture, scene di prova e cache).
- Non aggiungere a Git file di test o artefatti temporanei di verifica.
- Non eliminare file preesistenti estranei al lavoro corrente senza autorizzazione dell'utente. I test originali di Blender rimangono; la pulizia richiesta riguarda i test di sviluppo Bmax.
- Conservare la cache di compilazione, le dipendenze e i file runtime necessari alla build.

## Comportamenti da preservare

- Nuove armature e Add Bone in Edit Mode partono da 20 cm, compensando la scala delle unità della scena. Rispettare i valori espliciti; duplicazioni, estrusioni e ossa esistenti mantengono il loro comportamento. La scala dell'oggetto armatura continua ad applicarsi.
- Gli elementi coincidenti devono mantenere il colore selezionato, con priorità all'elemento selezionato attivo. Le armature usano buffer distinti per questa priorità; in Object Mode anche empty, curve legacy, lattice e geometria mesh senza facce coordinano la priorità dei rispettivi overlay.
- Non confondere il contorno con il puntino dell'origine dell'oggetto, che appartiene a un overlay separato.
- Preservare profondità, occlusione, clipping, In Front, X-Ray e picking GPU. Non cambiare la matematica di valutazione delle ossa per correggere la visualizzazione.
- Per modifiche ai contorni, verificare l'ordine inverso di creazione, la selezione multipla con oggetto attivo e le coppie di tipi diversi coinvolti. Verificare OpenGL/Vulkan, l'occlusione e il picking quando disponibili, usando solo artefatti temporanei.

## Ambiente Windows locale

- Checkout sorgenti: `D:\blender_prj`. Build: `D:\blender_build\octahedral_radius`. Eseguibile installato: `D:\blender_build\octahedral_radius\bin\blender.exe`.
- Il nome della directory di build è storico; compila l'intero Bmax. Usare Visual Studio 2022/MSVC v143 e il target CMake `INSTALL`, seguendo la guida; i target di test locali sono disabilitati.
- Una copia sul Desktop non viene aggiornata dall'installazione in questa directory. Non chiudere un'istanza dell'utente senza autorizzazione.
- Con modifiche locali non pubblicate, i metadati Blender possono indicare il commit del branch di tracking. Controllare sorgente, stato Git e build; non modificare il tracking o il sistema di build solo per cambiare la stringa della versione.
