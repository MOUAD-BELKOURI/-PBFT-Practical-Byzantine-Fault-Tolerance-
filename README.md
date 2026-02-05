# Implémentation et expérimentation du protocole PBFT avec MPI

**Auteur : MOUAD BELKOURI**

---

## 1. Introduction

Les systèmes distribués modernes sont de plus en plus utilisés dans des contextes critiques tels que les systèmes financiers, les bases de données distribuées, les réseaux blockchain et les infrastructures cloud. Dans ces environnements, la tolérance aux fautes est un enjeu majeur, en particulier face aux fautes dites *byzantines*, où certains nœuds peuvent se comporter de manière arbitraire ou malveillante.

Le protocole **PBFT (Practical Byzantine Fault Tolerance)**, proposé par Castro et Liskov, constitue une solution efficace permettant d’assurer le consensus dans un système distribué même en présence de fautes byzantines. Contrairement aux approches basées sur la preuve de travail, PBFT offre de bonnes performances et une latence réduite dans des environnements à nombre de nœuds limité.

L’objectif de ce projet est d’implémenter et d’expérimenter le protocole PBFT en utilisant **MPI (Message Passing Interface)** afin de simuler la communication entre pairs dans un système distribué.

---

## 2. Problématique du consensus distribué

Dans un système distribué, le consensus consiste à faire en sorte que tous les nœuds corrects s’accordent sur une même valeur, malgré la présence de défaillances. Le problème devient plus complexe lorsque l’on considère les fautes byzantines, où un nœud peut envoyer des messages incohérents ou incorrects.

Le théorème fondamental de PBFT stipule que pour tolérer **f** fautes byzantines, le système doit contenir au minimum :

```
N = 3f + 1
```

Cette condition garantit que les nœuds corrects sont majoritaires et peuvent imposer un consensus fiable.

---

## 3. Présentation du protocole PBFT

Le protocole PBFT repose sur un modèle **client–serveur** avec un ensemble de replicas, parmi lesquels un nœud est désigné comme **primary**.

Le protocole se déroule en plusieurs phases bien définies :

1. **PRE-PREPARE**
2. **PREPARE**
3. **COMMIT**
4. **REPLY**

Chaque phase implique des échanges de messages permettant de vérifier la cohérence des requêtes et d’atteindre un consensus.

---

## 4. Architecture générale de l’implémentation

Dans cette implémentation, **MPI** est utilisé comme couche de communication entre les différents processus.

### 4.1 Répartition des rôles

* **Processus 0 : Client**
* **Processus 1 : Primary**
* **Processus 2 à N : Replicas (backups)**

### 4.2 Hypothèses

* Vue fixe (`view = 0`)
* Primary fixe
* Une seule requête client
* Communication fiable assurée par MPI

---

## 5. Description détaillée des messages

Plusieurs types de messages sont définis pour implémenter PBFT :

* `request_t` : requête envoyée par le client
* `pre_prepare_t` : message PRE-PREPARE envoyé par le primary
* `prepare_t` : message PREPARE échangé entre replicas
* `commit_t` : message COMMIT échangé entre replicas
* `reply_t` : réponse envoyée au client

Chaque message contient les champs nécessaires à la vérification de la cohérence (vue, numéro de séquence, type de requête, identifiant du processus).

---

## 6. Déroulement du protocole

### 6.1 Phase PRE-PREPARE

Le client envoie une requête au primary. Celui-ci attribue un numéro de séquence et diffuse un message PRE-PREPARE à tous les replicas.

### 6.2 Phase PREPARE

Chaque replica vérifie la validité du message PRE-PREPARE puis diffuse un message PREPARE à l’ensemble des autres replicas. La condition `prepared()` est satisfaite lorsque **au moins 2f messages PREPARE valides** sont reçus.

### 6.3 Phase COMMIT

Après la phase PREPARE, chaque replica envoie un message COMMIT. La condition `committed_local()` est satisfaite lorsqu’**au moins 2f+1 messages COMMIT valides** sont reçus.

### 6.4 Phase REPLY

Une fois la requête exécutée, chaque replica envoie une réponse au client. Le client valide le consensus lorsqu’il reçoit **au moins f+1 réponses identiques**.

---

## 7. Implémentation technique

L’implémentation est réalisée en **C avec MPI**. Des types MPI personnalisés sont définis afin de transmettre efficacement les structures de données.

Les fonctions principales incluent :

* `execute()` : exécution déterministe de la requête
* `prepared()` : vérification du quorum PREPARE
* `committed_local()` : vérification du quorum COMMIT

---

## 8. Expérimentations et résultats

### 8.1 Environnement de test

Les expérimentations ont été réalisées sur une machine Linux (Ubuntu) en utilisant **OpenMPI**.

L’exécution du programme se fait via la commande suivante :

```
mpirun --oversubscribe -np X ./pbft_mpi
```

où `X` représente le nombre total de processus MPI (1 client + N replicas).

### 8.2 Test avec p = 5 processus (f = 1)

Dans ce scénario, le système contient **1 client et 4 replicas**. Selon la formule PBFT `N = 3f + 1`, le système peut tolérer jusqu’à **1 faute byzantine**.

Résultats observés :

* Tous les replicas reçoivent correctement le message PRE-PREPARE.
* Les messages PREPARE sont échangés entre les replicas.
* La condition `prepared()` est satisfaite.
* La condition `committed_local()` est satisfaite.
* Chaque replica exécute la requête et envoie un REPLY au client.
* Le client reçoit au moins `f+1` réponses identiques et valide le consensus.

### 8.3 Test avec p = 7 processus (f = 1)

Dans ce test, le nombre de replicas est porté à 6.

Les résultats confirment que :

* Le protocole reste correct même avec un plus grand nombre de nœuds.
* Les quorums PBFT sont respectés.
* Le consensus est atteint sans ambiguïté.

Ces tests démontrent la scalabilité relative du protocole PBFT pour un nombre modéré de nœuds.

---

## 9. Analyse détaillée du code source

### 9.1 Organisation générale

Le code est structuré autour de trois rôles distincts : **client, primary et replicas**. Chaque rôle est implémenté par une fonction dédiée.

### 9.2 Gestion des messages

Les structures C représentent fidèlement les messages PBFT. Des types MPI personnalisés sont créés pour assurer une communication correcte entre processus.

### 9.3 Vérification des quorums

Les fonctions `prepared()` et `committed_local()` implémentent les conditions théoriques du protocole PBFT.

### 9.4 Exécution déterministe

La fonction `execute()` garantit que tous les replicas corrects appliquent la même opération dans le même ordre.

---

## 10. Comparaison avec Raft et Paxos

### 10.1 PBFT vs Paxos

Paxos tolère uniquement les fautes par arrêt (*crash faults*) et ne prend pas en compte les comportements byzantins. PBFT est donc plus robuste mais aussi plus coûteux en communication.

### 10.2 PBFT vs Raft

Raft simplifie la compréhension du consensus mais reste limité aux fautes non byzantines. PBFT est mieux adapté aux environnements hostiles.

---

## 11. Limites et perspectives

Cette implémentation est volontairement simplifiée à des fins pédagogiques. Elle ne prend pas en compte :

* le mécanisme de **view-change**,
* les **signatures cryptographiques**,
* les **nœuds réellement byzantins**,
* la gestion de **multiples requêtes concurrentes**.

### Perspectives d’amélioration

* Implémentation du **view-change**
* Ajout de **signatures cryptographiques**
* Simulation de **nœuds byzantins réels**
* Support de **requêtes concurrentes**

---
![image alt](https://github.com/MOUAD-BELKOURI/-PBFT-Practical-Byzantine-Fault-Tolerance-/blob/main/pbft1.jpg?raw=true)

## 12. Conclusion

Ce projet a permis d’implémenter avec succès le protocole **PBFT** en s’appuyant sur **MPI** comme infrastructure de communication distribuée.

Les expérimentations ont démontré que le système atteint correctement le consensus, conformément au modèle `N = 3f + 1`.

Ce travail constitue une base solide pour des améliorations futures et une compréhension approfondie du consensus distribué et de la tolérance aux fautes byzantines.
