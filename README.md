```html
<h1 align="center">🚀 Implémentation et expérimentation du protocole PBFT avec MPI</h1>

<p align="center">
  <strong>👨‍💻 MOUAD BELKOURI</strong>  
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C-blue"/>
  <img src="https://img.shields.io/badge/Communication-MPI-green"/>
  <img src="https://img.shields.io/badge/Consensus-PBFT-red"/>
</p>

<hr>

<h2>📌 1. Introduction</h2>

<p>
Les systèmes distribués modernes sont de plus en plus utilisés dans des contextes critiques 
tels que les systèmes financiers, les bases de données distribuées, les réseaux blockchain 
et les infrastructures cloud.  
</p>

<p>
Le protocole <b>PBFT (Practical Byzantine Fault Tolerance)</b> permet d’assurer un consensus 
même en présence de nœuds malveillants (fautes byzantines).  
</p>

<p>
🎯 <b>Objectif du projet :</b> Implémenter et tester PBFT en utilisant <b>MPI</b> pour simuler 
la communication entre processus distribués.
</p>

<p align="center">
  <img src="https://upload.wikimedia.org/wikipedia/commons/5/5f/Byzantine_generals_problem.svg" width="400"/>
</p>

<hr>

<h2>🧠 2. Problème du consensus distribué</h2>

<p>
Pour tolérer <b>f</b> fautes byzantines, PBFT exige :
</p>

<pre>
N = 3f + 1
</pre>

<p>
Cela garantit qu’une majorité de nœuds honnêtes existe toujours.
</p>

<hr>

<h2>⚙️ 3. Phases du protocole PBFT</h2>

<ul>
  <li>🟡 <b>PRE-PREPARE</b></li>
  <li>🟠 <b>PREPARE</b></li>
  <li>🔴 <b>COMMIT</b></li>
  <li>🟢 <b>REPLY</b></li>
</ul>

<p align="center">
  <img src="https://miro.medium.com/max/1400/1*LmgYJpzb12Z0hU4bP8s1-g.png" width="500"/>
</p>

<hr>

<h2>🏗️ 4. Architecture du système</h2>

<h3>👥 Répartition des rôles</h3>

<ul>
  <li>🧑‍💻 <b>Processus 0</b> → Client</li>
  <li>👑 <b>Processus 1</b> → Primary</li>
  <li>🖥️ <b>Processus 2 à N</b> → Replicas</li>
</ul>

<h3>📋 Hypothèses</h3>

<ul>
  <li>Vue fixe</li>
  <li>Primary fixe</li>
  <li>Une seule requête client</li>
  <li>Communication fiable avec MPI</li>
</ul>

<hr>

<h2>📩 5. Types de messages</h2>

<ul>
  <li>📨 <b>request_t</b> : requête client</li>
  <li>📬 <b>pre_prepare_t</b> : message du primary</li>
  <li>🔄 <b>prepare_t</b> : échanges entre replicas</li>
  <li>✅ <b>commit_t</b> : validation finale</li>
  <li>📤 <b>reply_t</b> : réponse au client</li>
</ul>

<hr>

<h2>🔁 6. Déroulement du protocole</h2>

<h3>🟡 PRE-PREPARE</h3>
<p>Le client envoie une requête → le primary diffuse.</p>

<h3>🟠 PREPARE</h3>
<p>Les replicas échangent des messages PREPARE.</p>

<h3>🔴 COMMIT</h3>
<p>Validation finale par quorum.</p>

<h3>🟢 REPLY</h3>
<p>Les replicas répondent au client.</p>

<p align="center">
  <img src="https://www.researchgate.net/publication/341324088/figure/fig1/AS:892155316838915@1589565131120/The-PBFT-protocol-phases.png" width="500"/>
</p>

<hr>

<h2>💻 7. Implémentation</h2>

<p>Langage : <b>C</b> + <b>MPI</b></p>

<ul>
  <li>⚙️ <code>execute()</code> : exécute la requête</li>
  <li>📊 <code>prepared()</code> : vérifie le quorum</li>
  <li>✔️ <code>committed_local()</code> : validation locale</li>
</ul>

<hr>

<h2>🧪 8. Expérimentations</h2>

<h3>▶️ Lancer le programme</h3>

<pre>
mpirun --oversubscribe -np X ./pbft_mpi
</pre>

<h3>📊 Test 1 : 5 processus (f = 1)</h3>

<ul>
  <li>✅ PRE-PREPARE reçu</li>
  <li>✅ PREPARE échangé</li>
  <li>✅ COMMIT validé</li>
  <li>✅ Consensus atteint</li>
</ul>

<h3>📊 Test 2 : 7 processus</h3>

<ul>
  <li>🔝 Plus de redondance</li>
  <li>🔒 Consensus toujours correct</li>
</ul>

<hr>

<h2>📚 9. Comparaison</h2>

<table border="1" cellpadding="10">
  <tr>
    <th>Protocole</th>
    <th>Avantages</th>
    <th>Limites</th>
  </tr>
  <tr>
    <td>PBFT</td>
    <td>Résiste aux attaques byzantines</td>
    <td>Coûteux en communication</td>
  </tr>
  <tr>
    <td>Paxos</td>
    <td>Simple</td>
    <td>Pas byzantin</td>
  </tr>
  <tr>
    <td>Raft</td>
    <td>Très lisible</td>
    <td>Pas byzantin</td>
  </tr>
</table>

<hr>

<h2>🚧 10. Limites & Améliorations</h2>

<ul>
  <li>⚠️ Pas de view-change</li>
  <li>⚠️ Pas de signatures cryptographiques</li>
  <li>⚠️ Pas de nœuds byzantins réels</li>
</ul>

<h3>🚀 Perspectives</h3>

<ul>
  <li>🔄 Implémenter le view-change</li>
  <li>🔐 Ajouter des signatures</li>
  <li>🤖 Simuler des nœuds malveillants</li>
</ul>

<hr>

<h2>🎯 Conclusion</h2>

<p>
Ce projet a démontré qu’il est possible d’implémenter PBFT avec MPI et d’obtenir un consensus fiable
dans un système distribué. Il constitue une excellente base pour des travaux avancés en systèmes distribués.
</p>

<p align="center">⭐ <b>N’hésite pas à star ce projet !</b> ⭐</p>
```
