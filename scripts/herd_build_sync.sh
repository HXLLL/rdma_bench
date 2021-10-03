cd herd
make
cd ..

rsync -a --delete /root/rdma_bench/ cu02:/root/rdma_bench