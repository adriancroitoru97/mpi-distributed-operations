#include <stdio.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <mpi.h>

#define NUM_CLUSTERS 4
#define MASTER 0
#define ISOLATED 1
#define MAX_SIZE 1000000

using namespace std;

void read_workers_from_file(int cluster_rank, int &numworkers, int workers[])
{
    string cluster_file = "cluster" + to_string(cluster_rank) + ".txt";
    ifstream fin(cluster_file);

    fin >> numworkers;
    for (int i = 0; i < numworkers; i++)
    {
        int worker;
        fin >> worker;
        workers[i] = worker;
    }

    fin.close();
}

int next_cluster(int current)
{
    return (current + 1) % NUM_CLUSTERS;
}

int prev_cluster(int current)
{
    return current - 1 + ((current < 1) ? NUM_CLUSTERS : 0);
}

void insert_array_into_vector(vector<int> &v, int a[], int len)
{
    for (int i = 0; i < len; i++)
    {
        v.push_back(a[i]);
    }
}

void vector_to_array(vector<int> v, int a[], int &len)
{
    len = v.size();
    for (int i = 0; i < v.size(); i++) {
        a[i] = v[i];
    }
}

void print_topology(int rank, unordered_map<int, vector<int>> topology, int arg, int cluster)
{
    cout << rank << " -> ";

    /* Cluster 1 is isolated case */
    if (arg == 2 && (rank == ISOLATED || cluster == ISOLATED)) {
        if (cluster != -1) cout << cluster << ":";
        else cout << rank << ":";
        for (int j = 0; j < topology[1].size(); j++) {
            cout << topology[1][j];
            if (j < topology[1].size() - 1) {
                cout << ",";
            }
        }
        cout << "\n";
        return;
    }
    
    /* Normal case */
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        if (arg == 2 && i == ISOLATED) continue;
        cout << i << ":";
        for (int j = 0; j < topology[i].size(); j++) {
            cout << topology[i][j];
            if (j < topology[i].size() - 1) {
                cout << ",";
            }
        }
        cout << " ";
    }
    cout << "\n";
}

void log_message(int sender, int receiver) {
    cout << "M(" << sender << "," << receiver << ")\n";
}

/* Sends the workers of the `cluster` from `source` to `destination */
void send_cluster_workers(int source, int destination, int *cluster, int *len, int *v) {
    MPI_Send(cluster, 1, MPI_INT, destination, 0, MPI_COMM_WORLD);
    MPI_Send(len, 1, MPI_INT, destination, 1, MPI_COMM_WORLD);
    MPI_Send(v, *len, MPI_INT, destination, 2, MPI_COMM_WORLD);
    log_message(source, destination);
}

/* Receives the workers of the `cluster` from `source` to `destination */
void receive_cluster_workers(int source, int *rank_rcv, int *len, int *v) {
    MPI_Recv(rank_rcv, 1, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(len, 1, MPI_INT, source, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(v, *len, MPI_INT, source, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void send_array(int source, int destination, int *N, int *V) {
    MPI_Send(N, 1, MPI_INT, destination, 0, MPI_COMM_WORLD);
    MPI_Send(V, *N, MPI_INT, destination, 1, MPI_COMM_WORLD);
    log_message(source, destination);
}

void receive_array(int source, int *N, int *V) {
    MPI_Recv(N, 1, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(V, *N, MPI_INT, source, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void print_array(int N, int *V) {
    cout << "Rezultat: ";
    for (int i = 0; i < N; i++) {
        cout << V[i] << " ";
    }
    cout << "\n";
}

int get_num_workers(unordered_map<int, vector<int>> topology) {
    int num_workers = 0;
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        num_workers += topology[i].size();
    }
    return num_workers;
}

int get_cl_start_pos(unordered_map<int, vector<int>> topology, int cl_rank, int chunk_size) {
    int num_workers = 0;
    for (int i = 0; i < cl_rank; i++) {
        num_workers += topology[i].size();
    }
    return num_workers * chunk_size;
}

void copy_array_interval(int s[], int d[], int start, int end) {
    for (int i = start; i < end; i++) {
        d[i] = s[i];
    }
}

int main(int argc, char *argv[])
{
    int numtasks, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    unordered_map<int, vector<int>> topology;
    int N;
    int V[MAX_SIZE];

    /* Variable only used by workers to know their coordinator */
    int cluster = -1;

    /* CLUSTER TOPOLOGY SET */
    if (rank < NUM_CLUSTERS)
    {
        int numworkers, workers[numtasks];
        read_workers_from_file(rank, numworkers, workers);

        /* Populate isolated topology */
        if (stoi(argv[2]) == 2 && rank == ISOLATED) {
            insert_array_into_vector(topology[rank], workers, numworkers);
        }

        /* Populate normal topology */
        if (rank == MASTER) {
            insert_array_into_vector(topology[rank], workers, numworkers);

            /* Master cluster receives the workers of all other clusters and stores them */
            int receving_clusters = NUM_CLUSTERS - 1;
            if (stoi(argv[2]) == 2) receving_clusters = NUM_CLUSTERS - 2;
            for (int i = 0; i < receving_clusters; i++) {
                int rank_rcv;
                receive_cluster_workers(prev_cluster(rank), &rank_rcv, &numworkers, workers);
                insert_array_into_vector(topology[rank_rcv], workers, numworkers);
            }
            
            /* Master sends the full topology to the rest of clusters, ignoring the 0-1 link */
            for (int i = 0; i < NUM_CLUSTERS; i++) {
                vector_to_array(topology[i], workers, numworkers);
                send_cluster_workers(rank, prev_cluster(rank), &i, &numworkers, workers);
            }
        } else {
            /* Each non-master cluster sends the worker list to the master (forwarding process) */
            if (stoi(argv[2]) != 2 || rank != ISOLATED) {
                send_cluster_workers(rank, next_cluster(rank), &rank, &numworkers, workers);
            }

            /* Forwarding process from any cluster to master */
            if (rank != 1) {
                int receving_clusters = rank - 1;
                if (stoi(argv[2]) == 2) receving_clusters = rank - 2;
                for (int i = 0; i < receving_clusters; i++) {
                    int rank_rcv;
                    receive_cluster_workers(prev_cluster(rank), &rank_rcv, &numworkers, workers);
                    send_cluster_workers(rank, next_cluster(rank), &rank_rcv, &numworkers, workers);
                }
            }

            /* Each cluster receives the full topology from MASTER, considering
               the case when cluster 1 is ISOLATED */
            if (stoi(argv[2]) != 2 || rank != ISOLATED) {
                for (int i = 0; i < NUM_CLUSTERS; i++) {
                    int rank_rcv;
                    receive_cluster_workers(next_cluster(rank), &rank_rcv, &numworkers, workers);
                    insert_array_into_vector(topology[rank_rcv], workers, numworkers);

                    if (rank != 1 && (stoi(argv[2]) != 2 || prev_cluster(rank) != ISOLATED)) {
                        send_cluster_workers(rank, prev_cluster(rank), &rank_rcv, &numworkers, workers);
                    }
                }
            }
        }

        print_topology(rank, topology, stoi(argv[2]), -1);

        /* Send the topology to each worker of the current cluster */
        for (auto worker : topology[rank]) {
            /* Inform the worker about it's cluster */
            MPI_Send(&rank, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);
            log_message(rank, worker);
            
            /* Send to the worker the full topology */
            if (stoi(argv[2]) == 2 && rank == ISOLATED) {
                int cl = ISOLATED;
                vector_to_array(topology[ISOLATED], workers, numworkers);
                send_cluster_workers(rank, worker, &cl, &numworkers, workers);
            } else {
                for (int i = 0; i < NUM_CLUSTERS; i++) {
                    if (stoi(argv[2]) == 2 && i == 1) continue;
                    vector_to_array(topology[i], workers, numworkers);
                    send_cluster_workers(rank, worker, &i, &numworkers, workers);
                }
            }
        }
    }

    /* WORKER TOPOLOGY SET */
    if (rank >= NUM_CLUSTERS)
    {
        int numworkers, workers[numtasks];
        
        /* Receive the coordinator cluster */
        MPI_Status status;
        MPI_Recv(&cluster, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        cluster = status.MPI_SOURCE;

        /* Receive the topology from the coordinator */
        if (stoi(argv[2]) == 2 && cluster == ISOLATED) {
            int rank_rcv;
            receive_cluster_workers(cluster, &rank_rcv, &numworkers, workers);
            insert_array_into_vector(topology[rank_rcv], workers, numworkers);
        } else {
            int nrcl = NUM_CLUSTERS;
            if (stoi(argv[2]) == 2) nrcl--;
            for (int i = 0; i < nrcl; i++) {
                int rank_rcv;
                receive_cluster_workers(cluster, &rank_rcv, &numworkers, workers);
                insert_array_into_vector(topology[rank_rcv], workers, numworkers);
            }
        }

        print_topology(rank, topology, stoi(argv[2]), cluster);
    }

    /* CLUSTER ARRAY MANAGEMENT */
    if (rank < NUM_CLUSTERS)
    {
        /* Forward the array through the clusters ring */
        if (rank == MASTER)
        {
            N = stoi(argv[1]);
            for (int i = 0; i < N; i++) {
                V[i] = N - i - 1;
            }
            send_array(rank, prev_cluster(rank), &N, V);
        }
        else
        {
            if (stoi(argv[2]) != 2 || rank != ISOLATED) {
                receive_array(next_cluster(rank), &N, V);
                if (rank != 1 && (stoi(argv[2]) != 2 || prev_cluster(rank) != ISOLATED)) {
                    send_array(rank, prev_cluster(rank), &N, V);
                }
            }
        }

        if (stoi(argv[2]) != 2 || rank != ISOLATED) {
            /* Compute the equal management of operations for each worker */
            int num_workers = get_num_workers(topology);
            int chunk_size = N / num_workers;
            int cl_start_pos = get_cl_start_pos(topology, rank, chunk_size);

            /* Send the array to current cluster's workers */
            for (int i = 0; i < topology[rank].size(); i++) {
                int worker = topology[rank][i];
                int w_start_pos = cl_start_pos + chunk_size * i;
                send_array(rank, worker, &N, V);

                /* Forward all `surplus` work to the last worker */
                if (rank == NUM_CLUSTERS - 1 && i == topology[rank].size() - 1) {
                    chunk_size += (N % num_workers);
                }
                
                MPI_Send(&w_start_pos, 1, MPI_INT, worker, 2, MPI_COMM_WORLD);
                MPI_Send(&chunk_size, 1, MPI_INT, worker, 3, MPI_COMM_WORLD);
                log_message(rank, worker);

                /* Only last worker has a different chunk_size */
                if (rank == NUM_CLUSTERS - 1 && i == topology[rank].size() - 1) {
                    chunk_size -= (N % num_workers);
                }
            }

            /* Receive the computed array back from the workers */
            for (int i = 0; i < topology[rank].size(); i++) {
                int tmp_V[MAX_SIZE];
                int worker = topology[rank][i];
                int w_start_pos = cl_start_pos + chunk_size * i;
                int w_end_pos = w_start_pos + chunk_size;
                receive_array(worker, &N, tmp_V);

                /* Last worker takes all the remaining elements */
                if (rank == NUM_CLUSTERS - 1 && i == topology[rank].size() - 1) {
                    w_end_pos = N;
                }
                copy_array_interval(tmp_V, V, w_start_pos, w_end_pos);
            }

            /* Forward the computed arrays to the MASTER */
            if (rank == MASTER) {
                int receving_clusters = NUM_CLUSTERS - 1;
                if (stoi(argv[2]) == 2) receving_clusters = NUM_CLUSTERS - 2;
                for (int i = 0; i < receving_clusters; i++) {
                    int rank_rcv, tmp_V[MAX_SIZE];
                    MPI_Recv(&rank_rcv, 1, MPI_INT, prev_cluster(rank), 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    receive_array(prev_cluster(rank), &N, tmp_V);

                    /* Compute the equal management of operations for each worker */
                    int num_workers = get_num_workers(topology);
                    int chunk_size = N / num_workers;

                    int cl_spos = get_cl_start_pos(topology, rank_rcv, chunk_size);
                    int cl_epos = cl_spos + chunk_size * topology[rank_rcv].size();

                    /* Last cluster takes all the remaining elements */
                    if (rank_rcv == NUM_CLUSTERS - 1) {
                        cl_epos = N;
                    }
                    copy_array_interval(tmp_V, V, cl_spos, cl_epos);
                }
            } else {
                MPI_Send(&rank, 1, MPI_INT, next_cluster(rank), 2, MPI_COMM_WORLD);
                send_array(rank, next_cluster(rank), &N, V);
                int receving_clusters = rank - 1;
                if (stoi(argv[2]) == 2) receving_clusters = rank - 2;
                for (int i = 0; i < receving_clusters; i++) {
                    int rank_rcv;
                    MPI_Recv(&rank_rcv, 1, MPI_INT, prev_cluster(rank), 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    receive_array(prev_cluster(rank), &N, V);
                    MPI_Send(&rank_rcv, 1, MPI_INT, next_cluster(rank), 2, MPI_COMM_WORLD);
                    send_array(rank, next_cluster(rank), &N, V);
                }
            }
        }

        /* Print the result */
        if (rank == MASTER) {
            print_array(N, V);
        }
    }

    /* WORKER ARRAY MANAGEMENT */
    if (stoi(argv[2]) != 2 || cluster != ISOLATED) {
        if (rank >= NUM_CLUSTERS)
        {
            int w_start_pos, chunk_size;
            receive_array(cluster, &N, V);
            MPI_Recv(&w_start_pos, 1, MPI_INT, cluster, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&chunk_size, 1, MPI_INT, cluster, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = w_start_pos; i < w_start_pos + chunk_size; i++) {
                V[i] = V[i] * 5;
            }

            send_array(rank, cluster, &N, V);
        }
    }

    MPI_Finalize();
}
