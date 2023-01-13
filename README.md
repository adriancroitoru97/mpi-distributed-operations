# MPI Distributed Operations

Distributed program written in `C++`, using `mpi.h` library. The topology is
formed by 4 clusters connected in a ring and containing different number of
assigned workers. The main idea is to efficiently distribute
a mock operation (multiplying an array by 5) to all workers.

![Example topology](/samples/1.png "Example topology")


## Topology initialization

Initially, each cluster reads its worker file and send its workers to
the `MASTER` cluster, through the ring. So, a cluster will not only send
its workers, but also receive the previous cluster workers and forward them
to the `MASTER`.\
When the master has received the workers from all clusters, it will broadcast
the full topology to the ring (respecting the connections).\
When a cluster has received the full topology, it will broadcast it to
its workers, who will store it too.

All the logic has been made such as the connection between 0 and 1 processes
is never used - **3rd exercise**.

![Topology used](/samples/2.png "Topology used")


## Efficient distribution of operations

The `MASTER` cluster computes the initial array and sends it through the ring.
The forwarding process is similar to the **Topology Initialization** algorithm.
In the end, all clusters, and therefore, all workers,
have received the full array.

Each cluster calculates its **start point** in the array,
and the **chunk size** - the number of operations each worker has to do.
This information is sent to the workers, and each will manage its part of
the array accordingly.

The **array size** is divided by the number of workers in the topology in order
to find the chunk size. Because this division result cannot always be
an integer, the result is rounded, and all the remaining operations
are forwarded to the last worker.\
This strategy works because in real life scenarios, the number of operations
will be much larger than the number of workers **(N >> W)**,
and the last worker will at most have to do `W - 1` more operations
than the others.

The results of each worker are firstly computed back in each cluster,
and then the clusters send their parts to the `MASTER`.


## Isolated cluster (BONUS)

The cluster `1` has already the `0-1 link` broken,
as the implementation does not use this link at all.\
In order the broke the other one, `1-2`, and therefore, to completely separate
the cluster from the topology, there are special cases for this task.

When cluster `1` is separated, it will not send its workers to anyone,
and the `MASTER` process will receive one less **worker array** from the ring.
The **forwarding process** from all clusters will also require
one less **receive** from the previous clusters.

When it comes to the array management, the distribution of workload will not
consider the workers managed by cluster `1`, as it is not in the main topology.

![Bonus topology](/samples/3.png "bonus")

***


***Only one global communicator was used in the program,
but in real situations it might be better (and necessary) to use separate
communicators for each cluster.***


## License

[Adrian-Valeriu Croitoru](https://github.com/adriancroitoru97)
