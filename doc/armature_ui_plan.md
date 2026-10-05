# Armature UI: analisi e piano per Blender 5.2.2

Documento storico: i percorsi, gli script e gli stati delle verifiche riportati nelle sezioni del piano descrivono lo sviluppo iniziale. Dal 2026-10-05 il branch attivo e `main`; `codex/armature-ui` e `codex/bmax-publication` sono archiviati tramite tag. Per le prossime compilazioni seguire [Building Bmax on Windows](bmax/BUILD_WINDOWS.md).

Base: tag v5.2.2, commit d13f752e3b9c4f8c261cda552b1021f8bcc0382c.
Repository: D:\blender_prj. Branch attivo: main (branch storico iniziale: codex/armature-ui).
Stato: Octahedral Radius, hide/unhide sincronizzato, Names/Axis sulle ossa selezionate e Axis Size indipendente implementati. Bmax integra il logo fornito e usa le preferenze di Blender 5.2. Build Bmax precedente compilata e installata, sette regressioni precedentemente superate. Nuova build con Axis Size compilata e installata con successo. Verifica grafica interattiva ancora da eseguire.

## Elenco aggiornato delle feature

La lista completa delle funzionalita implementate in main dal 1 al 5 ottobre 2026 e in [Bmax feature inventory](bmax/FEATURES.md). Include le modifiche alle armature e al gesto Tab, gli assi dell'origine, i nuovi default dei modificatori, outline/overlap/flipped UV condivisi tra Object ed Edit Mode, opacita e inquadratura UV, Mesh Holes, branding, nuove ossa di 20 cm e contorni selezionati affidabili per elementi coincidenti, inclusi empty, curve, lattice e mesh con soli spigoli o punti in Object Mode. Il documento distingue le feature presenti, l'addon rimosso e gli interventi implementati.

Le modifiche del 2026-10-05 sono implementate e verificate: nuove ossa di 20 cm (`cebe29eb`) e priorita dei contorni selezionati anche per oggetti senza facce (`9e6f22f9`). Per le regole operative dell'agente vedere [AGENTS.md](../AGENTS.md); per le verifiche correnti usare la guida di build, non gli script storici citati sotto.

## Requisiti confermati

- Modifiche di interfaccia e visualizzazione delle armature.
- Nomi e assi filtrabili sulle ossa selezionate.
- Un raggio di visualizzazione assoluto per osso, condiviso da Octahedral e dalle due sfere alle estremita. Head e Root indicano queste sfere, non nuovi tipi di visualizzazione.
- Sincronizzazione di selezione e stato nascosto tra Pose ed Edit Mode, dopo proposta del comportamento.
- Nuove ossa: 0,2 unita Blender, equivalenti a 20 cm nella scena metrica standard.
- Elementi coincidenti: il contorno selezionato deve restare visibile; per piu elementi selezionati basta un contorno comune.
- Matematica delle ossa, constraint, solver e valutazione delle animazioni restano fuori dall'intervento.

## 1. Nomi e assi delle ossa selezionate

File:
- source/blender/makesdna/DNA_armature_types.h
- source/blender/makesrna/intern/rna_armature.cc
- scripts/startup/bl_ui/properties_data_armature.py
- source/blender/draw/engines/overlay/overlay_armature.cc

ARM_DRAWNAMES e ARM_DRAWAXES sono flag globali dell'armatura. I cicli draw_armature_edit e draw_armature_pose disegnano i nomi e gli assi di tutte le ossa visibili quando i flag sono attivi.

Implementazione Bmax richiesta successivamente: i toggle Names e Axis mostrano sempre e soltanto le ossa selezionate. In Pose si usa POSE_SELECTED tramite UnifiedBonePtr; in Edit conta anche la selezione della sola testa o coda. Un osso solamente attivo, ma deselezionato, non basta.

Fattibilita alta, impatto circoscritto ai filtri degli overlay. I filtri non devono alterare le query GPU di selezione.

## 2. Raggio assoluto di visualizzazione

File:
- source/blender/makesdna/DNA_armature_types.h (Bone)
- source/blender/blenkernel/BKE_armature.hh (EditBone)
- source/blender/makesrna/intern/rna_armature.cc
- scripts/startup/bl_ui/properties_data_bone.py
- source/blender/editors/armature/armature_utils.cc
- source/blender/editors/armature/armature_add.cc
- source/blender/blenloader/intern/versioning_520.cc
- source/blender/draw/engines/overlay/overlay_armature.cc
- source/blender/draw/engines/overlay/overlay_shape.cc

Oggi draw_bone_update_disp_matrix_default scala tutti e tre gli assi della matrice di visualizzazione con la lunghezza dell'osso. Octahedral e le sfere usano questa matrice. La sfera base ha raggio 0,05; quindi il raggio visivo attuale cresce con la lunghezza.

Proposta: nuovo campo display_radius su Bone/EditBone, esposto come distanza. Coordinate locali dell'armatura: indipendente dalla lunghezza; la scala dell'oggetto puo ancora influenzare la dimensione nel mondo, come per le larghezze B-Bone. Un solo valore controlla il raggio massimo trasversale dell'Octahedral e il raggio delle due sfere.

Costruire matrici di disegno dedicate: asse longitudinale determinato da testa/coda, assi trasversali determinati dal raggio, sfere con scala uniforme e centri sulle vere estremita. Non modificare indiscriminatamente la matrice comune: serve anche ad assi, B-Bone, custom shape e altri overlay. Le geometrie usate nel picking devono coincidere con quelle visibili.

Il valore deve passare da Bone a EditBone e viceversa; verificare duplicazione, undo e salvataggio. Non riutilizzare rad_head/rad_tail, perche sono raggi degli Envelope e possono partecipare alla deformazione. Non riutilizzare xwidth/zwidth: appartengono ai B-Bone.

Fattibilita alta; richiede modifica dei dati salvati e migrazione dei vecchi file. Una sola misura applicata identicamente a larghezza e sfere cambia le proporzioni visive storiche, quindi non e possibile mantenere entrambe identiche al passato. La migrazione deve esplicitare questa scelta e inizializzare il valore una sola volta, senza legarlo dinamicamente alla lunghezza. Proposta iniziale per nuovi ossi di 0,2: raggio 0,02, modificabile per osso.

Casi delicati: osso molto corto con raggio grande, pose con scale non uniformi/negative, oggetti scalati, endpoint connessi che Blender normalmente omette, file aperti in Blender standard che non dispone della nuova proprieta.

## 4. Stato attuale e proposta Pose/Edit

File:
- source/blender/editors/object/object_edit.cc
- source/blender/editors/armature/armature_utils.cc
- source/blender/editors/armature/armature_edit.cc
- source/blender/editors/armature/pose_edit.cc
- source/blender/animrig/ANIM_armature.hh
- source/blender/makesrna/intern/rna_pose.cc

In questa versione la selezione Pose usa flag POSE_SELECTED sul bPoseChannel, che appartiene all'oggetto. Edit usa BONE_SELECTED/BONE_ROOTSEL/BONE_TIPSEL su EditBone. Le funzioni flush_pose_selection_to_bone e flush_bone_selection_to_pose, chiamate entrando/uscendo da Edit, trasferiscono gia la selezione, inclusi i flag delle estremita. make_boneList_recursive e ED_armature_from_edit copiano i dati tra Bone/EditBone e sistemano le selezioni dei punti connessi.

La visibilita non usa lo stesso stato: Pose usa PCHAN_DRAW_HIDDEN, Edit usa BONE_HIDDEN_A. I flag BONE_HIDDEN_P ancora presenti non vanno confusi con il campo Pose corrente. In piu, la visibilita delle Bone Collections e un filtro distinto. Gli operatori Hide/Reveal agiscono sui rispettivi stati e deselezionano le ossa nascoste.

Comportamento proposto prima dell'implementazione:
1. Entrando in Edit, copiare selezione e nascosto dell'oggetto che guida il passaggio ai dati Edit.
2. Tornando in Pose, copiare indietro nascosto e selezione, conservando la selezione parziale di testa/coda quando il sistema gia lo permette.
3. Uno stato nascosto non puo mantenere selezione; correggere l'osso attivo se diventa nascosto o non valido.
4. Reveal conserva la scelta dell'operatore sulla selezione delle ossa rivelate.
5. Non tradurre la visibilita delle collezioni in uno Hide per osso: altrimenti si perderebbe la distinzione tra le due cause di invisibilita.
6. Per oggetti che condividono l'armatura, Edit ha un unico stato condiviso: l'oggetto attivo guida l'ingresso. Trasferire all'uscita agli oggetti che hanno realmente partecipato al passaggio di modo, senza propagare lo stato indiscriminatamente a tutte le istanze. Verificare l'iterazione e la deduplicazione dei dati in multi-object Edit prima della patch definitiva.

Fattibilita buona ma rischio maggiore dei semplici filtri: coinvolge selezione, undo, dati condivisi e stato attivo. Prima cercare un difetto concreto nel trasferimento gia presente; non sostituire la selezione di Blender con un secondo sistema parallelo.

## 5. Nuove ossa lunghe 20 cm

Ci sono almeno due percorsi da aggiornare:
- ARMATURE_OT_bone_primitive_add in armature_add.cc: la proprieta length ha default 1,0.
- OBJECT_OT_armature_add in object_add.cc: usa la proprieta radius e la passa a ED_armature_ebone_add_primitive.

Proposta: default 0,2 per l'osso iniziale di una nuova armatura e per Add Bone in Edit, preservando i valori espliciti passati dagli script e il pannello dell'operatore. Esaminare WM_operator_view3d_unit_defaults per evitare doppie conversioni quando Unit Scale non vale 1.

Non forzare a 0,2 ossa duplicate, estrusioni verso il cursore o ossa importate: cambierebbe la geometria del rig. Estrusioni e duplicazioni conservano il comportamento corrente.

## 6. Elementi coincidenti e contorni

Armature:
- source/blender/draw/engines/overlay/overlay_armature.hh
- source/blender/draw/engines/overlay/overlay_armature.cc
- source/blender/draw/engines/overlay/shaders/overlay_armature_shape_outline_vert.glsl
- source/blender/draw/engines/overlay/shaders/overlay_armature_sphere_outline_vert.glsl

Oggetti:
- source/blender/draw/engines/overlay/overlay_instance.cc
- source/blender/draw/engines/overlay/overlay_outline.hh
- source/blender/draw/engines/overlay/shaders/overlay_outline_detect_frag.glsl

Le armature disegnano direttamente riempimenti e contorni con buffer separati; diversi sottopass usano DEPTH_LESS_EQUAL e scrittura del depth. L'ordine delle istanze e la scrittura dei contorni sono candidati per una sovrascrittura a pari profondita. E un'ipotesi da riprodurre, non una causa dimostrata.

L'outline degli oggetti ha invece un prepass di ID con una propria profondita, alimentato dagli oggetti selezionati. Il resolve confronta la sua profondita con quella della scena; esiste gia un epsilon per le coincidenze. Per questo una perdita completa del contorno degli oggetti non si puo attribuire automaticamente alla stessa causa delle ossa.

Obiettivo confermato: almeno un selezionato coincidente deve avere contorno visibile, anche se un elemento non selezionato vince il disegno del riempimento; piu selezionati possono condividere il contorno. Conservare la priorita visiva dell'elemento attivo ove applicabile. Elementi realmente dietro altri, a profondita diversa, conservano le normali regole di occlusione/X-Ray/In Front.

Piano di indagine:
1. Riproduzione minima: due ossa identiche nella stessa armatura, due armature coincidenti, due mesh identiche; provare prima selezione singola e poi multipla, variando ordine di creazione e selezione.
2. Confrontare Edit/Pose/Object, ortografica/prospettiva, Solid/Material Preview, In Front, X-Ray, custom shape e backend GPU effettivamente disponibili.
3. Controllare se i contorni selezionati vengono sovrascritti da contorni non selezionati; prototipare una priorita esplicita delle istanze selezionate/attive nei soli overlay delle armature.
4. Se necessario, provare un pass dedicato al contorno selezionato con un confronto di profondita circoscritto. Non usare depth test sempre superato: farebbe apparire davanti anche ossa realmente coperte.
5. Intervenire sul percorso generale degli oggetti soltanto se la riproduzione ne dimostra il difetto. Cambiare l'epsilon generale o il depth buffer globale senza questa verifica e prematuro.

Rischi: flicker nelle superfici quasi coincidenti, contorni visibili attraverso oggetti non coincidenti, differenze tra GPU, colori attivo/selezionato sovrascritti, costi di pass aggiuntivi. La prima verifica deve essere grafica, non soltanto un test numerico.

## Ordine di implementazione e verifica

1. Completare gli asset LFS necessari e le librerie precompilate windows_x64 corrispondenti al tag. Usare l'aggiornamento delle sole dipendenze, evitando di avanzare dalla base 5.2.2 al codice corrente di upstream.
2. Compilare e conservare una build di base, riprodurre i problemi e acquisire confronti grafici.
3. Implementare i filtri nomi/assi e relazioni, poi i due default di creazione; verificare che selezione e lunghezza degli ossi preesistenti non cambino.
4. Implementare raggio, copia dei dati e migrazione; verificare stessa dimensione con lunghezze diverse, salvataggio/riapertura, undo e duplicazione.
5. Implementare la sincronizzazione proposta dopo la verifica dei casi condivisi, con controlli su Hide/Reveal, punti connessi, ossa attive, multi-object Edit e collezioni nascoste.
6. Correggere il contorno con il piu piccolo intervento dimostrato necessario dalla riproduzione.
7. Compilare la build modificata e confrontare con la base usando un rig con constraint e animazione: trasformazioni valutate identiche per il medesimo rig; nuova geometria solo per gli operatori di creazione richiesti.

## Ambiente e stato effettivo

Visual Studio 2022 17.14, MSVC v143, Windows SDK, CMake 4.4.3 e Git sono rilevabili. CMake non e ancora rilevato nel PATH della sessione corrente, ma l'eseguibile e disponibile in C:\Program Files\CMake\bin\cmake.exe; e presente anche la copia inclusa in Visual Studio.

Il checkout iniziale si e fermato sugli asset LFS ed e stato completato con i puntatori Git LFS. I sorgenti sono disponibili; i binari degli asset non sono ancora tutti scaricati. lib/windows_x64 non contiene ancora le librerie precompilate richieste. Non e stata eseguita una compilazione e non e stato verificato il risultato visivo.

upstream punta al repository ufficiale di Blender. La branch e locale; non e stato pubblicato un fork remoto.


## Prima implementazione: Octahedral Radius

Parametro RNA octahedral_radius su Bone/EditBone, etichetta Octahedral Radius in N > Item. Default 0,02 unita locali dell'armatura. Definisce la semilarghezza X/Z dell'Octahedral e il raggio delle sfere. Indipendente dalla lunghezza e dalle scale della posa; la scala dell'oggetto continua ad applicarsi. Le forme custom e gli altri tipi di visualizzazione conservano i rispettivi controlli.

La matrice comune di disegno resta invariata: l'Octahedral usa una copia con scala trasversale dedicata; le sfere usano matrici uniformi centrate sulle estremita. Le stesse geometrie alimentano il picking. Il raggio e copiato nei passaggi Bone/EditBone. La migrazione usa la presenza del membro DNA per distinguere un file standard da un file gia salvato con la build personalizzata; inizializza il raggio con la semilarghezza Octahedral precedente, una sola volta.

Verifiche eseguite durante lo sviluppo: trasformazioni rest/pose con constraint invarianti, raggio conservato nei passaggi di modo e nei cambi di lunghezza, duplicazione, salvataggio/riapertura. Lo script temporaneo di verifica e stato rimosso dalla distribuzione e dal CMake dei test Python.

## Esito della prima build

Build Release completata con Visual Studio 2022 e CMake. Il generatore DNA ha rilevato il padding aggiuntivo necessario nel Bone; aggiunti quattro byte espliciti e completata la compilazione. Librerie Windows e asset necessari alla build scaricati.

Eseguibile: D:\blender_build\octahedral_radius\bin\blender.exe
Scena di confronto: D:\blender_build\octahedral_radius\octahedral_radius_demo.blend
Log build: D:\blender_build\octahedral_radius\build.log
Log verifiche: D:\blender_build\octahedral_radius\Testing\Temporary\LastTest.log

CTest bl_armature_octahedral_radius superato. Tutti i cinque controlli interni eseguiti, senza skip: duplicazione; passaggio Edit/Pose e cambio di lunghezza; matrici rest e pose valutata con constraint inalterate; salvataggio/riapertura; migrazione di un file standard con armature. La scena di confronto contiene ossa da 0,2 / 0,8 / 2 unita con raggio comune 0,02, in scena metrica.

La build non include i kernel GPU precompilati CUDA/HIP/OneAPI. L'OptiX SDK non e installato. La modifica del raggio e stata verificata su dati e compilazione; non e ancora stata controllata graficamente in una viewport interattiva. Gli altri interventi del piano e la correzione dei contorni coincidenti non sono stati implementati in questa prima patch.


## Aggiornamento Bmax (1 ottobre 2026)

- Nome applicazione Windows: Bmax; eseguibili bmax.exe e bmax-launcher.exe.
- Logo PNG fornito incorporato in splash e About; icone Windows multirisoluzione.
- Preferenze e startup: percorso Blender 5.2 condiviso, come richiesto.
- Names e Axis: i master toggle restano, ma mostrano solo le ossa selezionate.
  In Edit anche un endpoint selezionato conta come selezione.
- Assi: ciascuna linea dall'origine al marcatore misura 3 * Octahedral Radius,
  in spazio armatura. Posizione sul segmento invariata; scala oggetto applicata.
- Hide/unhide: trasferimento dello stato al passaggio Edit/Pose in entrambi i sensi,
  tramite i flussi esistenti della selezione. Ossa nascoste deselezionate;
  la visibilita delle collezioni resta un filtro distinto.
- Le proposte precedenti Selected Only e dimensione assi separata sono superate
  dalle richieste Bmax. Constraint, matrici di animazione e shader generali invariati.


### Controllo visivo della build Bmax

Scena preparata: D:/blender_build/octahedral_radius/bmax_armature_demo.blend.
Eseguibile: D:/blender_build/octahedral_radius/bin/bmax.exe.

1. In Pose selezionare una delle tre ossa: solo questa mostra nome e assi.
2. N > Item > Octahedral Radius: modificare il raggio e verificare che corpo,
   sfere e assi cambino insieme. Ogni asse misura tre volte il raggio.
3. Cambiare lunghezza in Edit: larghezza e assi conservano la dimensione.
4. Nascondere con H in Pose, passare in Edit: l'osso resta nascosto.
5. Alt+H in Edit, tornare in Pose: l'osso torna visibile.
6. Ripetere H in Edit e Alt+H in Pose, cambiando modo dopo ciascuna operazione.
7. Disattivare Names o Axis: il rispettivo overlay scompare anche sui selezionati.
8. Verificare titolo Bmax, splash/About e icona nella barra Windows.

La verifica automatica copre i dati e i passaggi di modo; il disegno nella
viewport richiede il controllo visivo indicato sopra.

Build finale verificata: Bmax 5.2.2 LTS, Windows x64, branch codex/armature-ui.
Launcher e percorso di configurazione Blender 5.2 verificati in background.


## Scala della posa nell'Octahedral (1 ottobre 2026)

Il raggio resta una dimensione di base indipendente dalla lunghezza a riposo.
In Pose la matrice valutata viene applicata anche alla larghezza X/Z e alle
sfere: scala non uniforme, scala ereditata e shear tornano visibili come in
Blender ufficiale. Anche la scala uniforme della posa agisce sulla forma.
La lunghezza Y segueva gia la posa. Assi e proprieta del raggio conservano
il comportamento Bmax concordato; solver e matrici di animazione invariati.


## Conferma del pie dei modi al rilascio

VIEW3D_OT_object_mode_pie_or_toggle ora ha un callback invoke che passa al pie
l'evento di apertura effettivo, come WM_OT_call_menu_pie. Il percorso exec
conserva il fallback per le chiamate programmatiche. Quando apre il menu il
comando restituisce OPERATOR_INTERFACE. Il rilascio della scorciatoia puo
cosi confermare la voce puntata secondo le normali preferenze dei pie.
Il gesto richiede verifica interattiva sulla scorciatoia Tab dell'utente.


### Riscontro successivo: Tab for Pie Menu

L'utente usa use_v3d_tab_menu (Tab for Pie Menu), che genera wm.call_menu_pie
con name VIEW3D_MT_object_mode_pie, evento TAB/PRESS. Questo percorso non usa
VIEW3D_OT_object_mode_pie_or_toggle: la precedente modifica non risolve quindi
il caso riportato. Preferenze lette: use_v3d_tab_menu=True,
use_pie_click_drag=False, pie_tap_timeout=20.

Confronto con la distribuzione ufficiale locale 5.2.2, hash d13f752e3b9c:
blender.py, blender_default.py e la definizione del menu dei modi sono identici.
Non sono state apportate modifiche al percorso wm.call_menu_pie o ai gestori
comuni dei pie. Causa effettiva non ancora accertata.

Avvio diagnostico pronto: D:/blender_build/octahedral_radius/diagnose_mode_pie.cmd.
Registra preferenze, associazioni Tab ed eventi/handler del gesto, senza salvare
preferenze. Serve riproduzione dell'utente nella nuova finestra e chiusura per
completare D:/blender_build/octahedral_radius/bmax_mode_pie_diagnostic.log.


## Gesto richiesto dopo il tocco di Tab

Requisito chiarito: Tab PRESS/RELEASE, poi puntare una voce e oltrepassarne
il bordo esterno per eseguire, senza clic. Il log contiene tre coppie Tab
PRESS/RELEASE prima del movimento, mentre i pie RightMouse ricevono il
rilascio dopo il movimento. pie_menu_confirm=0 nel profilo Bmax.

Implementazione circoscritta al menu VIEW3D_MT_object_mode_pie aperto con Tab:
flag runtime PIE_CONFIRM_AFTER_TAP; conferma quando il puntatore supera il
bordo esterno della voce attiva di 4 pixel scalati con la UI. La conferma
attende il termine dell'animazione e funziona anche sul timer. Le voci
disabilitate non vengono attivate. Preferenze e altri pie invariati.
La precedente modifica di VIEW3D_OT_object_mode_pie_or_toggle e stata ritirata.
Richiesta verifica del gesto interattivo nella build risultante.


## Axis Size indipendente

Nuova richiesta: Axis Size per osso sotto Octahedral Radius in N > Item,
in Edit e Pose. Lunghezza assoluta predefinita 0,03 unita Blender (3 cm
nella scena metrica standard), indipendente dal raggio e dalla lunghezza.
Nuove ossa e file privi del campo ricevono 0,03; i valori personalizzati
si conservano in Bmax e nei passaggi di modo. Sostituisce la regola 3x raggio.
Come Octahedral Radius, il nuovo campo non viene conservato risalvando il
file da Blender ufficiale. La scala dell'oggetto si applica alla visualizzazione.


## Publication status (supersedes earlier historical notes)

Published as MaxPuliero/Bmax. The current executables are blender.exe and blender-launcher.exe; branding remains Bmax. Axis Size is independent, default 0.03 units. The previously bundled Max Puliero Pie Menu List addon has been removed. New bones now default to 20 cm and coincident armature selection outlines use selected/active drawing priority. See the English root README for the current feature list.
