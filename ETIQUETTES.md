# Étiquettes et mini-manuel — boîtier compteur de passages

Pense-bête à imprimer et coller sur ou près du boîtier, pour ne pas avoir à
rouvrir la doc technique (`README.md`) une fois le boîtier fermé. Le
comportement décrit ici est le même que dans `README.md` (section
« Fonctionnement »), juste reformulé pour un usage courant.

## 1. Planche d'étiquettes à découper

Chaque bloc ci-dessous est pensé comme une étiquette indépendante, à recopier
sur une étiqueteuse / des autocollants, à coller près de l'élément concerné.

---

**BOUTON A**
- Appui court → affiche la distance mesurée (2 s)
- Appui long (~1 s) → ouvre le menu réglages
- *Dans le menu* : change le capteur (ultrason / laser ToF)

---

**BOUTON B**
- Appui court → affiche la distance mesurée (2 s)
- Appui long (~1 s) → ouvre le menu réglages
- *Dans le menu* : change la largeur de détection (capteur ToF)

---

**LES DEUX BOUTONS ENSEMBLE**
- Maintenir 5 secondes → remise à zéro du compteur
- Un décompte (5...1) s'affiche pendant l'attente : relâcher avant la fin
  annule la remise à zéro

---

**AFFICHEUR — ce que montre l'écran**
- Un nombre fixe → le compteur de passages
- Un petit serpent qui tourne → calibration en cours (laisser le passage
  vide, ne pas toucher)
- Les deux points `:` allumés → capteur bloqué (quelque chose reste immobile
  devant le capteur)
- Un nombre à 2 chiffres qui clignote → menu réglages ouvert
- Un décompte qui descend (5→1) → remise à zéro en cours

---

## 2. Notice (une page)

### Utilisation courante

Le boîtier compte automatiquement chaque passage devant le capteur ;
aucune action n'est nécessaire en usage normal. Le nombre affiché est le
nombre total de passages comptés.

### Voir la distance mesurée (appui court)

Un appui court sur **le bouton A ou le bouton B** affiche pendant 2 secondes
la distance actuellement mesurée par le capteur, et allume brièvement le
laser de visée. Utile pour vérifier que le boîtier est bien orienté, sans
rien dérégler.

### Ouvrir le menu réglages (appui long)

Un appui d'environ 1 seconde sur **un seul bouton** (A ou B) ouvre le menu :
l'écran affiche un nombre à 2 chiffres qui clignote.
- Le **bouton A** change le chiffre des dizaines : le capteur utilisé
  (`1` = ultrason, `2` = laser ToF).
- Le **bouton B** change le chiffre des unités : la largeur de détection du
  capteur laser (`1` = large, `2` = moyenne, `3` = étroite).

Ne rien toucher pendant 4 secondes referme le menu et enregistre les choix.
Si le capteur a changé, une nouvelle calibration démarre automatiquement
(le petit serpent tourne à l'écran — laisser le passage vide jusqu'à ce
qu'il s'arrête).

### Remettre le compteur à zéro

Maintenir **les deux boutons appuyés en même temps pendant 5 secondes**.
Un décompte (5, 4, 3, 2, 1) s'affiche à l'écran pour savoir où on en est ;
relâcher avant la fin annule l'opération sans rien changer. Un bip confirme
la remise à zéro.

### Si l'écran affiche les deux points `:` allumés

Le capteur est « bloqué » : quelque chose reste immobile devant lui depuis
trop longtemps (objet posé, personne arrêtée). Le comptage reprend
automatiquement dès que le passage est de nouveau libre.

### Si le serpent tourne sans s'arrêter

Le capteur actif ne renvoie aucune mesure valide (câblage, capteur ToF non
branché, ou objet juste devant le capteur). Vérifier le passage et le
câblage, ou rebasculer sur l'autre capteur via le menu.
