# building script for eRPC on infiniband machines
Dir.chdir('./third-party/eRPC') do
  system 'mkdir ./build' unless Dir.exist?('./build')
  Dir.chdir('./build') do
    return unless system 'cmake .. -DTRANSPORT=infiniband -DROCE=on -DPERF=on -DAZURE=off'

    return unless system 'make -j'

    return unless system 'cp ./src/config.h ../src'
  end
end
