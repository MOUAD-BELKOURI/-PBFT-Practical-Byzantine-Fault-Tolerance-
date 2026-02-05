#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <mpi.h>

/* ---------- Constantes générales ---------- */

#define CLIENT   0
#define PRIMARY  1   /* primary fixe pour la vue 0 */

#define ADD  1
#define SUB  2

/* Tags MPI pour séparer les types de messages */
#define TAG_REQUEST      100
#define TAG_PRE_PREPARE  200
#define TAG_PREPARE      300
#define TAG_COMMIT       400
#define TAG_REPLY        500

/* ---------- Types de messages PBFT ---------- */

/* Requête du client -> primary / replicas */
typedef struct {
    double timestamp;
    int    request_type;  /* ADD ou SUB */
} request_t;

/* PRE-PREPARE : primary -> replicas */
typedef struct {
    int view;
    int sequence_number;
    int request_type;
    int process_id;   /* id du primary */
} pre_prepare_t;

/* PREPARE : tous les replicas -> tous */
typedef struct {
    int view;
    int sequence_number;
    int request_type;
    int process_id;   /* id de l'émetteur */
} prepare_t;

/* COMMIT : tous les replicas -> tous */
typedef struct {
    int view;
    int sequence_number;
    int request_type;
    int process_id;   /* id de l'émetteur */
} commit_t;

/* REPLY : replicas -> client */
typedef struct {
    int view;
    int process_id;   /* replica qui répond */
    double timestamp; /* timestamp de la requête */
    int result;       /* état du service après exécution */
} reply_t;

/* ---------- Variables globales ---------- */

int my_rank, p;          /* p = nombre total de processus MPI */
int f;                   /* nombre de fautes byzantines tolérées */
int view = 0;            /* vue courante, fixée à 0 pour simplifier */
int state = 0;           /* état du service répliqué */

MPI_Datatype mpi_request;
MPI_Datatype mpi_pre_prepare;
MPI_Datatype mpi_prepare;
MPI_Datatype mpi_commit;
MPI_Datatype mpi_reply;

/* ---------- Utilitaires PBFT ---------- */

void execute(const request_t *req) {
    switch (req->request_type) {
        case ADD:
            state++;
            break;
        case SUB:
            state--;
            break;
        default:
            /* No-op */
            break;
    }
}

/*
 * prepared(): vérifie que le replica a reçu au moins 2f messages PREPARE
 * valides (en plus de son propre prepare implicite) pour (view, seq, type).
 */
int prepared(int sequence_number, int request_type) {
    int num_replicas = p - 1;  /* ranks 1..p-1 */
    int received[/* max p */ 64]; /* tableau indexé par rank, p max 64 ici */
    int i;

    if (p > 64) {
        /* Sécurité grossière, à adapter si besoin */
        if (my_rank == CLIENT) {
            fprintf(stderr, "Erreur: trop de processus (p > 64)\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (i = 0; i < p; i++) {
        received[i] = 0;
    }

    /* On considère qu'on a déjà traité notre propre PREPARE */
    received[my_rank] = 1;

    prepare_t prep;
    MPI_Status status;

    /* Réception d'un PREPARE de chaque autre replica */
    for (i = 1; i < p; i++) {
        if (i == my_rank) continue;

        MPI_Recv(&prep, 1, mpi_prepare, i, TAG_PREPARE, MPI_COMM_WORLD, &status);

        int ok = (prep.process_id == status.MPI_SOURCE &&
                  prep.view == view &&
                  prep.sequence_number == sequence_number &&
                  prep.request_type == request_type);

        if (ok) {
            received[i] = 1;
        }
    }

    int matching_others = 0;
    for (i = 1; i < p; i++) {
        if (i == my_rank) continue;
        if (received[i]) matching_others++;
    }

    /* Condition PBFT : au moins 2f PREPARE d'autres replicas */
    return (matching_others >= 2 * f);
}

/*
 * committed_local(): vérifie qu'on a reçu au moins 2f+1 COMMIT valides
 * (y compris le nôtre) pour (view, seq, type).
 */
int committed_local(int sequence_number, int request_type) {
    int num_replicas = p - 1;
    int received[64];
    int i;

    if (p > 64) {
        if (my_rank == CLIENT) {
            fprintf(stderr, "Erreur: trop de processus (p > 64)\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (i = 0; i < p; i++) {
        received[i] = 0;
    }

    /* On considère qu'on a déjà notre propre COMMIT */
    received[my_rank] = 1;

    commit_t com;
    MPI_Status status;

    /* Réception d'un COMMIT de chaque autre replica */
    for (i = 1; i < p; i++) {
        if (i == my_rank) continue;

        MPI_Recv(&com, 1, mpi_commit, i, TAG_COMMIT, MPI_COMM_WORLD, &status);

        int ok = (com.process_id == status.MPI_SOURCE &&
                  com.view == view &&
                  com.sequence_number == sequence_number &&
                  com.request_type == request_type);

        if (ok) {
            received[i] = 1;
        }
    }

    int count = 0;
    for (i = 1; i < p; i++) {
        if (received[i]) count++;
    }

    /* Condition PBFT : au moins 2f+1 COMMIT (y compris soi-même) */
    return (count >= 2 * f + 1);
}

/* ---------- Rôle client ---------- */

void client() {
    int num_replicas = p - 1;

    request_t req;
    reply_t rep;
    MPI_Status status;

    req.timestamp = MPI_Wtime();
    req.request_type = ADD;  /* opération fixe pour l'exemple */

    /* On envoie la requête au primary (rank 1) */
    MPI_Send(&req, 1, mpi_request, PRIMARY, TAG_REQUEST, MPI_COMM_WORLD);

    /* On attend les réponses des replicas */
    int max_replies = num_replicas;
    int *results = (int *)malloc(max_replies * sizeof(int));
    int replies_received = 0;
    int i;

    for (i = 0; i < max_replies; i++) {
        results[i] = INT_MIN;
    }

    while (replies_received < max_replies) {
        MPI_Recv(&rep, 1, mpi_reply, MPI_ANY_SOURCE, TAG_REPLY,
                 MPI_COMM_WORLD, &status);

        /* Vérification basique de cohérence */
        if (rep.view != view || rep.timestamp != req.timestamp ||
            rep.process_id != status.MPI_SOURCE || rep.result == INT_MIN) {
            /* On ignore cette réponse suspecte */
            continue;
        }

        results[replies_received++] = rep.result;
    }

    /* On cherche un résultat qui apparaît au moins f+1 fois */
    int consensus_found = 0;
    for (i = 0; i < max_replies; i++) {
        if (results[i] == INT_MIN) continue;
        int candidate = results[i];
        int count = 0;
        int j;
        for (j = 0; j < max_replies; j++) {
            if (results[j] == candidate) count++;
        }
        if (count >= f + 1) {
            printf("[Client] Consensus atteint: résultat = %d\n", candidate);
            consensus_found = 1;
            break;
        }
    }

    if (!consensus_found) {
        printf("[Client] Consensus non atteint!\n");
    }

    free(results);
}

/* ---------- Rôle primary ---------- */

void primary() {
    request_t req;
    MPI_Status status;

    /* Réception de la requête du client */
    MPI_Recv(&req, 1, mpi_request, CLIENT, TAG_REQUEST,
             MPI_COMM_WORLD, &status);

    printf("[Primary %d] Requête reçue: ts=%f, type=%d\n",
           my_rank, req.timestamp, req.request_type);

    int sequence_number = 1;  /* une seule requête pour l'exemple */

    /* PHASE PRE-PREPARE */
    pre_prepare_t pp;
    pp.view = view;
    pp.sequence_number = sequence_number;
    pp.request_type = req.request_type;
    pp.process_id = my_rank;

    /* Envoi du PRE-PREPARE + requête à tous les autres replicas */
    for (int i = 1; i < p; i++) {
        if (i == my_rank) continue;
        MPI_Send(&pp, 1, mpi_pre_prepare, i, TAG_PRE_PREPARE, MPI_COMM_WORLD);
        MPI_Send(&req, 1, mpi_request, i, TAG_REQUEST, MPI_COMM_WORLD);
    }

    printf("[Primary %d] PRE-PREPARE envoyé.\n", my_rank);

    /* PHASE PREPARE : le primary participe comme replica */
    prepare_t prep;
    prep.view = view;
    prep.sequence_number = sequence_number;
    prep.request_type = req.request_type;
    prep.process_id = my_rank;

    /* Envoi de PREPARE à tous les autres replicas */
    for (int i = 1; i < p; i++) {
        if (i == my_rank) continue;
        MPI_Send(&prep, 1, mpi_prepare, i, TAG_PREPARE, MPI_COMM_WORLD);
    }

    printf("[Primary %d] PREPARE envoyé.\n", my_rank);

    /* Réception des PREPARE des autres, et vérification du prédicat prepared() */
    if (!prepared(sequence_number, req.request_type)) {
        printf("[Primary %d] prepared() non satisfait, on devrait lancer un view-change (non implémenté ici).\n", my_rank);
        /* Pour simplifier, on continue quand même. */
    } else {
        printf("[Primary %d] prepared() satisfait.\n", my_rank);
    }

    /* PHASE COMMIT */
    commit_t com;
    com.view = view;
    com.sequence_number = sequence_number;
    com.request_type = req.request_type;
    com.process_id = my_rank;

    /* Envoi de COMMIT à tous les autres replicas */
    for (int i = 1; i < p; i++) {
        if (i == my_rank) continue;
        MPI_Send(&com, 1, mpi_commit, i, TAG_COMMIT, MPI_COMM_WORLD);
    }

    printf("[Primary %d] COMMIT envoyé.\n", my_rank);

    /* Réception des COMMIT des autres, et vérification de committed_local() */
    if (!committed_local(sequence_number, req.request_type)) {
        printf("[Primary %d] committed_local() non satisfait.\n", my_rank);
        /* On pourrait décider de ne pas exécuter, mais pour l'exemple, on continue. */
    } else {
        printf("[Primary %d] committed_local() satisfait.\n", my_rank);
    }

    /* Exécution de la requête et envoi du REPLY au client */
    execute(&req);

    reply_t rep;
    rep.view = view;
    rep.process_id = my_rank;
    rep.timestamp = req.timestamp;
    rep.result = state;

    MPI_Send(&rep, 1, mpi_reply, CLIENT, TAG_REPLY, MPI_COMM_WORLD);
    printf("[Primary %d] REPLY envoyé au client. state=%d\n", my_rank, state);
}

/* ---------- Rôle replica (backup) ---------- */

void replica() {
    MPI_Status status;
    request_t req;
    pre_prepare_t pp;

    /* PHASE PRE-PREPARE : réception du PRE-PREPARE du primary */
    MPI_Recv(&pp, 1, mpi_pre_prepare, PRIMARY, TAG_PRE_PREPARE,
             MPI_COMM_WORLD, &status);

    int sequence_number = pp.sequence_number;

    /* Vérification simple */
    if (pp.view != view || pp.process_id != PRIMARY) {
        printf("[Replica %d] PRE-PREPARE invalide!\n", my_rank);
        /* Dans un vrai PBFT, on lancerait un view-change. Ici, on continue quand même. */
    } else {
        printf("[Replica %d] PRE-PREPARE reçu et accepté.\n", my_rank);
    }

    /* Réception de la requête associée */
    MPI_Recv(&req, 1, mpi_request, PRIMARY, TAG_REQUEST,
             MPI_COMM_WORLD, &status);

    if (req.request_type != pp.request_type) {
        printf("[Replica %d] Requête ne correspond pas au PRE-PREPARE!\n", my_rank);
    }

    /* PHASE PREPARE */
    prepare_t prep;
    prep.view = view;
    prep.sequence_number = sequence_number;
    prep.request_type = req.request_type;
    prep.process_id = my_rank;

    /* Envoi de PREPARE à tous les autres replicas */
    for (int i = 1; i < p; i++) {
        if (i == my_rank) continue;
        MPI_Send(&prep, 1, mpi_prepare, i, TAG_PREPARE, MPI_COMM_WORLD);
    }

    printf("[Replica %d] PREPARE envoyé.\n", my_rank);

    if (!prepared(sequence_number, req.request_type)) {
        printf("[Replica %d] prepared() non satisfait.\n", my_rank);
    } else {
        printf("[Replica %d] prepared() satisfait.\n", my_rank);
    }

    /* PHASE COMMIT */
    commit_t com;
    com.view = view;
    com.sequence_number = sequence_number;
    com.request_type = req.request_type;
    com.process_id = my_rank;

    for (int i = 1; i < p; i++) {
        if (i == my_rank) continue;
        MPI_Send(&com, 1, mpi_commit, i, TAG_COMMIT, MPI_COMM_WORLD);
    }

    printf("[Replica %d] COMMIT envoyé.\n", my_rank);

    if (!committed_local(sequence_number, req.request_type)) {
        printf("[Replica %d] committed_local() non satisfait.\n", my_rank);
    } else {
        printf("[Replica %d] committed_local() satisfait.\n", my_rank);
    }

    /* Exécution et REPLY au client */
    execute(&req);

    reply_t rep;
    rep.view = view;
    rep.process_id = my_rank;
    rep.timestamp = req.timestamp;
    rep.result = state;

    MPI_Send(&rep, 1, mpi_reply, CLIENT, TAG_REPLY, MPI_COMM_WORLD);
    printf("[Replica %d] REPLY envoyé au client. state=%d\n", my_rank, state);
}

/* ---------- main() ---------- */

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    if (p < 5) {
        if (my_rank == 0) {
            fprintf(stderr,
                    "Il faut au moins 1 client + 4 replicas (p >= 5) pour PBFT (f >= 1).\n");
        }
        MPI_Finalize();
        return 1;
    }

    /* N = p - 1 = 3f + 1 => f = (p - 2) / 3 */
    f = (p - 2) / 3;

    if (my_rank == 0) {
        printf("[Client] p=%d, f=%d, replicas=%d\n", p, f, p - 1);
    }

    /* Création des types MPI */

    /* request_t */
    {
        int block_lengths[2] = {1, 1};
        MPI_Aint displs[2];
        MPI_Datatype types[2] = {MPI_DOUBLE, MPI_INT};

        displs[0] = offsetof(request_t, timestamp);
        displs[1] = offsetof(request_t, request_type);

        MPI_Type_create_struct(2, block_lengths, displs, types, &mpi_request);
        MPI_Type_commit(&mpi_request);
    }

    /* reply_t */
    {
        int block_lengths[4] = {1, 1, 1, 1};
        MPI_Aint displs[4];
        MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_DOUBLE, MPI_INT};

        displs[0] = offsetof(reply_t, view);
        displs[1] = offsetof(reply_t, process_id);
        displs[2] = offsetof(reply_t, timestamp);
        displs[3] = offsetof(reply_t, result);

        MPI_Type_create_struct(4, block_lengths, displs, types, &mpi_reply);
        MPI_Type_commit(&mpi_reply);
    }

    /* pre_prepare_t, prepare_t, commit_t : mêmes champs (4 int) */
    MPI_Type_contiguous(4, MPI_INT, &mpi_pre_prepare);
    MPI_Type_commit(&mpi_pre_prepare);

    MPI_Type_contiguous(4, MPI_INT, &mpi_prepare);
    MPI_Type_commit(&mpi_prepare);

    MPI_Type_contiguous(4, MPI_INT, &mpi_commit);
    MPI_Type_commit(&mpi_commit);

    /* Répartition des rôles */
    if (my_rank == CLIENT) {
        client();
    } else if (my_rank == PRIMARY) {
        primary();
    } else {
        replica();
    }

    MPI_Type_free(&mpi_request);
    MPI_Type_free(&mpi_reply);
    MPI_Type_free(&mpi_pre_prepare);
    MPI_Type_free(&mpi_prepare);
    MPI_Type_free(&mpi_commit);

    MPI_Finalize();
    return 0;
}
