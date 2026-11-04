# Bmax: istruzioni per gli agenti AI

## Repository e documentazione

- Questo repository contiene il fork Bmax di Blender 5.2.3. Il branch di riferimento è `main`; il remoto di pubblicazione è `origin` (`https://github.com/MaxPuliero/Bmax.git`). Non pubblicare modifiche nel repository originale di Blender.
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
- Il corpo Octahedral ha base quadrata piatta e punta, raccordate al bordo delle sfere Head/Tail. Non cambiare geometria, centri, raggio o picking delle sfere. Le Bendy Bones in Octahedral mantengono corpi dei segmenti collegati alle posizioni reali dei segmenti vicini, senza sfere o distacchi interni; solo gli endpoint esterni si raccordano al raggio. Omettere i corpi interamente coperti dalle sfere, senza invertirli. Il display B-Bone a scatole resta separato; preservare valutazione, scala e shear.
- Nomi e assi delle ossa sono nascosti in Object Mode senza cambiare le opzioni salvate Names/Axes; tornando in Edit/Pose rispettano le scelte e il filtro delle ossa selezionate. Gli assi abilitati delle ossa selezionate in Edit/Pose si disegnano sempre davanti alla geometria e alle ossa, anche con In Front disabilitato sull'armatura. Usare il passaggio finale dedicato senza test o scrittura della profondità; preservare clipping, dimensioni indipendenti, filtri di selezione e percorso del picking.
- Mesh Holes usa metà dello spessore del contorno del tema, con minimo di 2 pixel fisici. Preservare occlusione, X-Ray, In Front e la regola topologica delle boundary edges. Mantenere etichetta e tooltip tradotti in giapponese, italiano, francese e spagnolo nei cataloghi PO e verificare i cataloghi installati dopo INSTALL.
- Gli elementi coincidenti devono mantenere il colore selezionato, con priorità all'elemento selezionato attivo. Le armature usano buffer distinti per questa priorità; in Object Mode anche empty, curve legacy, lattice e geometria mesh senza facce coordinano la priorità dei rispettivi overlay.
- Hair Curves ha controlli tema dedicati Width/Selected/Unselected in Preferenze > Temi > Vista 3D > Hair Curves. Lo spessore è in pixel fisici (default 4); i colori predefiniti sono giallo intenso e giallo scuro. Rispettare selezione Point/Curve, selezione morbida e Cage Opacity in Sculpt Mode. Le guide sono quelle editabili; non modificare raggio fisico, geometria, materiali o strand generati da Geometry Nodes. Non collegare questi controlli ai colori NURBS o alla selezione degli oggetti. I campi usano padding ThemeSpace esistente, senza alterare dimensioni o offset dei membri preesistenti; zero nello spessore è il sentinel per migrare le vecchie preferenze.
- Non confondere il contorno con il puntino dell'origine dell'oggetto, che appartiene a un overlay separato.
- Preservare profondità, occlusione, clipping, In Front, X-Ray e picking GPU per corpi e contorni; gli assi delle ossa fanno eccezione per la sola occlusione visiva. Non cambiare la matematica di valutazione delle ossa per correggere la visualizzazione.
- Per modifiche ai contorni, verificare l'ordine inverso di creazione, la selezione multipla con oggetto attivo e le coppie di tipi diversi coinvolti. Verificare OpenGL/Vulkan, l'occlusione e il picking quando disponibili, usando solo artefatti temporanei.

## Editing multiplo

- Le proprietà compatibili e le invocazioni UI dei comandi su modificatori, materiali e Geometry Data agiscono sulla selezione anche con Alt premuto. Non reintrodurre Alt come eccezione per il solo attivo. Preservare gli argomenti espliciti use_selected_objects e il comportamento dell'esecuzione Python diretta, i dati condivisi, annullamento e clipboard di array.

## Continuità viewport tra workspace

- Il cambio workspace trasferisce navigazione e visibilità dall'ultima viewport 3D usata alla corrispondente ultima viewport di destinazione, con fallback alla più grande. Preservare shading, overlay, altre viewport e orientamenti bloccati in Quad View; non copiare risorse di disegno o timer tra regioni.
- Local View e Local Collections devono avere slot nativi indipendenti e backup con proprietà separate. Il ritorno da Local View conserva la vista precedente all'isolamento. Rispettare i limiti nativi e segnalare l'esaurimento degli slot.
- Non sincronizzare tra scene diverse fissate ai workspace. Il tracking del focus è solo runtime con verifica dell'appartenenza di area/regione e identificatori di sessione; non aggiungere proprietà salvate per ricordare l'ultima viewport.

## Quick Favorites

- Il gestore in Preferences > Interface modifica direttamente U.user_menus: preservare le liste native bUserMenu/bUserMenuItem e il formato delle preferenze, senza un secondo archivio Bmax o nuovi campi DNA. Riordino e rimozione devono conservare parametri degli operatori, contesto di invocazione, enum, nomi personalizzati, percorsi e indici delle proprietà; usare le funzioni native per liberare le voci.
- Separare le liste per editor e chiavi native CTX_data_mode_string (objectmode, mesh_edit, armature_edit, ecc.); per i nodi usare il tree_idname. Non usare i nomi differenti di bpy.context.mode come chiavi salvate. Mantenere i gruppi ereditati da Top Bar e Properties e riordinare entro la rispettiva lista. Le selezioni del pannello sono stato temporaneo WindowManager, non preferenze aggiuntive.
- Le voci mancanti devono restare gestibili senza eseguire comandi o richiedere il loro contesto di esecuzione. Segnalare le modifiche con il normale dirty state delle preferenze. Verificare riordino/rimozione, salvataggio/ricaricamento e compatibilità con Blender ufficiale della stessa versione tramite sole copie temporanee delle preferenze.

## Save as Official

- File > Save as Official deve convertire solo una copia in un processo isolato. Preservare nome, dirty state, modalità, layer, immagini e risorse del progetto aperto. Il percorso nativo bmax_snapshot è nascosto e valido solo per Save As Copy; non cambiare il normale salvataggio di Blender.
- Convertire il weighting delle normali in custom normals standard; bake dei layer nella geometria Multires mantenendo i livelli. Il backup colore separato contiene mesh statiche valutate e normali attributi colore, non griglie Bmax editabili. Non sovrascrivere backup colore esistenti.
- Instances Random richiede un warning chiaro sulle differenze in Blender ufficiale, senza conversione automatica dei materiali. Non promettere compatibilità di add-on, cache esterne o funzionalità Bmax non convertite.
- Verificare la lettura in Blender ufficiale della stessa versione, il risultato geometrico/colore e che il progetto originale resti invariato. Se il runtime standard è occupato, mantenere la sessione dell'utente e installare un runtime completo separato; documentare i percorsi distinti.

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
