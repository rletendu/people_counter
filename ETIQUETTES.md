# Étiquettes et mini-manuel — boîtier compteur de passages

Pense-bête à imprimer et coller sur ou près du boîtier, pour ne pas avoir à
rouvrir la doc technique (`README.md`) une fois le boîtier fermé. Le
comportement décrit ici est le même que dans `README.md` (sections
« Démarrage rapide » et « Récapitulatif des boutons »), juste reformulé pour
un usage courant.

## 1. Planche d'étiquettes à découper

Chaque bloc ci-dessous est pensé comme une étiquette indépendante, à recopier
sur une étiqueteuse / des autocollants, à coller près de l'élément concerné.

---

**BOUTON A**
- Appui court → affiche la distance mesurée (~1 s) + laser de visée
- Appui long (~1 s) → menu **capteur**
- *Dans le menu capteur* : change le capteur (ultrason / laser ToF)
- *Dans le menu seuil* : seuil −5 cm

---

**BOUTON B**
- Appui court → affiche la distance mesurée (~1 s) + laser de visée
- Appui long (~1 s) → menu **seuil**
- *Dans le menu capteur* : change la largeur de détection (capteur ToF)
- *Dans le menu seuil* : seuil +5 cm

---

**LES DEUX BOUTONS ENSEMBLE**
- Maintenir 5 secondes → remise à zéro du compteur
- Un décompte `ooo5`…`ooo1` s'affiche pendant l'attente : relâcher avant la
  fin annule la remise à zéro

---

**À LA MISE SOUS TENSION** (maintenir pendant le branchement)
- Bouton A seul → verrouille / déverrouille les menus (`LOC` / `OPEn`)
- Les deux boutons → mode silencieux / sons rétablis (`8888` / `0`)

---

**AFFICHEUR — ce que montre l'écran**
- Un nombre fixe → le compteur de passages
- Un petit serpent qui tourne → calibration en cours (laisser le passage
  vide, ne pas toucher)
- Un nombre avec les `:` qui clignotent vite → distance mesurée (cm)
- `----` → aucune mesure valide (ou calibration ratée)
- Les deux points `:` allumés fixes → capteur bloqué (quelque chose reste
  immobile devant le capteur)
- Un nombre à 2 chiffres qui clignote → menu capteur ouvert
- Un nombre à 4 chiffres qui clignote → menu seuil ouvert
- Un décompte `ooo5`→`ooo1` → remise à zéro en cours
- `LOC` → menus verrouillés

---

## 2. Notice (une page)

### Mise en route

Brancher le boîtier **avec le passage vide**. Un petit serpent tourne à
l'écran et le laser de visée s'allume : le boîtier mesure la distance au mur
d'en face. Un bip confirme, la distance au mur s'affiche 2 secondes, puis le
compteur apparaît. Il reprend là où il en était avant la coupure.

### Utilisation courante

Le boîtier compte automatiquement chaque passage devant le capteur (un bip
court à chaque passage) ; aucune action n'est nécessaire en usage normal. Le
nombre affiché est le nombre total de passages : une personne qui entre puis
ressort compte 2.

### Voir la distance mesurée (appui court)

Un appui court sur **le bouton A ou le bouton B** affiche pendant environ
1 seconde la distance actuellement mesurée par le capteur (les `:`
clignotent pour la distinguer du compteur), et allume le laser de visée.
Utile pour vérifier que le boîtier est bien orienté, sans rien dérégler. Le
comptage continue pendant ce temps.

### Changer de capteur (appui long sur A)

Un appui d'environ 1 seconde sur **le bouton A seul** ouvre le menu capteur :
l'écran affiche un nombre à 2 chiffres qui clignote.
- Le **bouton A** change le chiffre des dizaines : le capteur utilisé
  (`1` = ultrason, `2` = laser ToF).
- Le **bouton B** change le chiffre des unités : la largeur de détection du
  capteur laser (`1` = large, `2` = moyenne, `3` = étroite).

Ne rien toucher pendant 4 secondes referme le menu et enregistre les choix.
Si le capteur a changé, une nouvelle calibration démarre automatiquement
(le petit serpent tourne à l'écran — laisser le passage vide jusqu'à ce
qu'il s'arrête).

### Régler le seuil à la main (appui long sur B)

Un appui d'environ 1 seconde sur **le bouton B seul** ouvre le menu seuil :
l'écran affiche un nombre à 4 chiffres qui clignote (le seuil de détection,
en cm).
- Le **bouton A** retire 5 cm, le **bouton B** ajoute 5 cm.
- `0000` = retour au seuil automatique (le boîtier se recalibre : passage
  vide).

Ne rien toucher pendant 4 secondes referme le menu et enregistre la valeur.
Attention : ouvrir ce menu puis le laisser se refermer sans revenir à
`0000` fixe le seuil à la main, et le boîtier ne se recalibrera plus tout
seul.

### Remettre le compteur à zéro

Maintenir **les deux boutons appuyés en même temps pendant 5 secondes**.
Un décompte (`ooo5`, `ooo4` … `ooo1`) s'affiche à l'écran pour savoir où on
en est ; relâcher avant la fin annule l'opération sans rien changer. Deux
notes graves confirment la remise à zéro.

### Verrouiller les menus / couper le son

Ces deux réglages se font **à la mise sous tension** : débrancher, maintenir
le ou les boutons, rebrancher, garder appuyé environ 1 seconde.
- **Bouton A seul** : verrouille les menus (`LOC` clignote) ou les
  déverrouille (`OPEn`). Menus verrouillés, un appui long fait un bip grave
  et affiche `LOC` ; l'aperçu distance et la remise à zéro restent possibles.
- **Les deux boutons** : mode silencieux (`8888` clignote) ou retour des sons
  (`0` clignote).

### Si l'écran affiche les deux points `:` allumés

Le capteur est « bloqué » : quelque chose reste immobile devant lui depuis
trop longtemps (objet posé, personne arrêtée). Le comptage reprend
automatiquement dès que le passage est de nouveau libre.

### Si `----` s'affiche après le serpent

La calibration a échoué (deux notes descendantes) : le capteur actif ne
renvoie aucune mesure valide (câblage, capteur ToF non branché, objet juste
devant le capteur, mur trop loin). Rien n'est compté dans ce cas. Vérifier
le passage et le câblage puis rebrancher, ou rebasculer sur l'autre capteur
via le menu capteur (appui long sur A).
