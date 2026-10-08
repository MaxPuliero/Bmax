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
- Nuovi oggetti armatura usano Octahedral, In Front e display oggetto Wire. Le nuove ossa usano Octahedral indipendentemente dal tipo di display dell'armatura; Add Bone abilita In Front e Wire sull'oggetto esistente. Preservare i tipi di display delle ossa esistenti e quelli copiati da duplicazione ed estrusione.
- Gli assi abilitati delle ossa selezionate si disegnano sempre davanti alla geometria e alle ossa, anche con In Front disabilitato sull'armatura. Usare il passaggio finale dedicato senza test o scrittura della profondità; preservare clipping, dimensioni indipendenti, filtri di selezione e percorso del picking.
- Mesh Holes usa metà dello spessore del contorno del tema, con minimo di 2 pixel fisici. Preservare occlusione, X-Ray, In Front e la regola topologica delle boundary edges. Mantenere etichetta e tooltip tradotti in giapponese, italiano, francese e spagnolo nei cataloghi PO e verificare i cataloghi installati dopo INSTALL.
- Gli elementi coincidenti devono mantenere il colore selezionato, con priorità all'elemento selezionato attivo. Le armature usano buffer distinti per questa priorità; in Object Mode anche empty, curve legacy, lattice e geometria mesh senza facce coordinano la priorità dei rispettivi overlay.
- Non confondere il contorno con il puntino dell'origine dell'oggetto, che appartiene a un overlay separato.
- Preservare profondità, occlusione, clipping, In Front, X-Ray e picking GPU per corpi e contorni; gli assi delle ossa fanno eccezione per la sola occlusione visiva. Non cambiare la matematica di valutazione delle ossa per correggere la visualizzazione.
- Per modifiche ai contorni, verificare l'ordine inverso di creazione, la selezione multipla con oggetto attivo e le coppie di tipi diversi coinvolti. Verificare OpenGL/Vulkan, l'occlusione e il picking quando disponibili, usando solo artefatti temporanei.

## Colore Multires in Sculpt Mode

- Leggere `doc/bmax/multires_color.md` prima di intervenire sul colore Multires. La feature comprende Paint, Blur e Smear dentro Sculpt Mode; il Vertex Paint classico resta separato. Color Filter, Mask by Color e Dynamic Topology non sono supportati da questo percorso.
- Il colore persistente usa `CD_GRID_PAINT_COLOR` e `GridPaintColor`, associati per nome all'attributo visibile. Preservare RGBA, copia e liberazione dei buffer annidati, serializzazione, rinomina/rimozione degli attributi e conversione Mesh/BMesh. I layer legacy possono avere lo stesso nome degli attributi ordinari: risolverli anche per tipo, senza interpretarli come array di colori della mesh base.
- Supportare Float/Byte e Point/Face Corner. I campioni Multires sono float RGBA anche per un attributo base Byte; applicare il modificatore produce un normale attributo Float Color sul dominio Face Corner.
- Cambiare livello senza dipingere deve conservare il dettaglio massimo. Una pennellata a livello inferiore propaga il delta al dettaglio conservato a fine gesto; Delete Higher elimina esplicitamente i livelli superiori. Undo/redo deve ripristinare esattamente sia i campioni correnti sia gli array persistenti ad alta risoluzione.
- Limitare elaborazione, undo e aggiornamenti GPU ai nodi raggiunti dal pennello e ai vicini necessari per i confini delle griglie. Non reintrodurre copie dell'intera superficie, salvataggi di tutte le griglie o ridisegni globali a ogni passo del pennello. Riutilizzare i buffer e salvare le griglie modificate a fine pennellata.
- Una pennellata solo colore non modifica le coordinate Multires e non deve provocare la ricostruzione completa del CCG tra pennellate. Preservare gli aggiornamenti necessari per rendering esterno, mesh condivise e uscita da Sculpt Mode. Ancoraggio e annullamento ripristinano i colori runtime senza riscrivere il dettaglio persistente a ogni passo.
- Per modifiche a questo percorso, verificare su una mesh con molti nodi PBVH: confini continui, maschere, undo/redo locale, livelli e pittura a livello inferiore, Blur/Smear, ancoraggio, attributi separati, rinomina/rimozione, salvataggio e applicazione del modificatore. Verificare viewport OpenGL/Vulkan e lettura del colore dal materiale quando il disegno o l'invalidazione cambiano.
- Confrontare le prestazioni con la stessa scena e pennellata, indicando riscaldamento, numero di misure e mediana. Il benchmark documentato misura l'operatore sincrono, non il frame rate interattivo; i risultati non sono una garanzia universale. Usare solo artefatti temporanei esterni al repository e rimuoverli dopo la verifica.

## Commit e pubblicazione

- Seguire lo stile dei commit recenti: titolo concreto in inglese, all'imperativo, per esempio `Add performant Multires color painting in Sculpt Mode`. Descrivere il comportamento finale; separare implementazione e aggiornamento documentale quando serve a citare l'hash della feature nell'inventario.
- Committare e pubblicare quando richiesto dall'utente. Controllare diff, branch e remoto; usare `origin` sul repository Bmax e verificare l'allineamento del branch remoto dopo il push. Non usare force push o pubblicare su `upstream` senza istruzioni esplicite.
- I commit del sorgente, il binario installato localmente, le copie sul Desktop e il download pubblico hanno stati distinti. Aggiornare README, inventario, note tecniche e guida di build senza confonderli. Non committare binari, cache di compilazione o artefatti temporanei.

## Ambiente Windows locale

- Checkout sorgenti: `D:\blender_prj`. Build: `D:\blender_build\octahedral_radius`. Eseguibile installato: `D:\blender_build\octahedral_radius\bin\blender.exe`.
- Il nome della directory di build è storico; compila l'intero Bmax. Usare Visual Studio 2022/MSVC v143 e il target CMake `INSTALL`, seguendo la guida; i target di test locali sono disabilitati.
- Una copia sul Desktop non viene aggiornata dall'installazione in questa directory. Non chiudere un'istanza dell'utente senza autorizzazione.
- Con modifiche locali non pubblicate, i metadati Blender possono indicare il commit del branch di tracking. Controllare sorgente, stato Git e build; non modificare il tracking o il sistema di build solo per cambiare la stringa della versione.
- Dopo modifiche al layout DNA o a strutture C++ condivise, ricompilare tutti i componenti con `/t:Rebuild` prima di INSTALL, senza eliminare la directory e le dipendenze della build. Verificare anche Boolean Manifold e Sculpt Trim: librerie compilate con layout precedenti possono linkare ma causare crash.
