cd write_tput
make
cd ..

rsync -a --delete /root/rdma_bench/ cu02:/root/rdma_bench
rsync -a --delete /root/rdma_bench/ cu03:/root/rdma_bench
rsync -a --delete /root/rdma_bench/ cu04:/root/rdma_bench

rsync -a --delete /root/rdma_bench/ dpu:/home/ubuntu/rdma_bench; ssh dpu bash -c "'cd /home/ubuntu/rdma_bench/write_tput; make clean; make'"