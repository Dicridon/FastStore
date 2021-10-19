# building script for eRPC on infiniband machines
Dir.chdir('./third-party/eRPC') do
  return unless system 'cmake . -DTRANSPORT=infiniband -DROCE=on -DPERF=on -DAZURE=off'

  return unless system 'make -j'

  return unless system 'cp ./src/config.h ../src'
end
