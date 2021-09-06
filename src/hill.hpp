namespace Hill {
    /*
     * This engine manages all RDMA connections, communications with monitor and the whole PM resource on one node
     *
     * PM is divided as follows
     * |----------------------------|
     * |                            |
     * |      Thread-local WAL      |
     * |                            |
     * |----------------------------|
     * |                            |
     * |   Local Memory Allocator   |
     * |                            |
     * |  ------------------------  |
     * |    Remote Memory Agent     |
     * |----------------------------|
     * |                            |
     * |        Data Region         |
     * |                            |
     * |                            |
     * |          ......            |
     * |                            |
     * |                            |
     * |----------------------------|
     */
    class Engine {
        
    };
}
