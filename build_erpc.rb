# building script for eRPC on infiniband machines
Dir.chdir('./third-party/eRPC') do
  return unless system 'cmake . -DTRANSPORT=infiniband -DROCE=on -DPERF=on -DCONFIG_IS_AZURE=0'

  return unless system 'make -j'
end
