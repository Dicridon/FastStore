require 'socket'

class Server
  def initialize(port)
    @port = port
  end

  def run
    s = TCPServer.open(@port)
    loop do
      Thread.start(s.accept) do |client|
        loop do
          content = client.gets.chop.to_i
          puts "replying #{content + 1}"
          client.puts(content + 1)
          sleep(1)
        end
      end
    end
  end
end

class Client
  def initialize(host, port)
    @host = host
    @port = port
  end

  def run
    s = TCPSocket.open(@host, @port)
    counter = 0
    puts "sending #{counter}"
    s.puts(counter)
    while (line = s.gets)
      puts "got #{line}"
      c = line.chop.to_i
      puts "sending #{c + 1}"
      s.puts(c)
    end
  end
end

class Parser
  attr_reader :is_server, :hostname, :port

  def initialize
    @args = ARGV.join(' ')
    @is_server = true
    @hostname = '127.0.0.1'
    @port = 2333
  end

  def parse
    ret, status = parse_is_server
    @is_server = ret if status
    ret, status = parse_hostname
    @hostname = ret if status
    ret, status = parse_port
    @port = ret if status
  end

  def parse_is_server
    @is_server = false if @args.match(/-s\s*(F|f)(alse)?|0/)
  end

  def parse_hostname
    mat = @args.match(/-h\s*((\d{1,3}.){3}\d{1,3})/)
    @hostname = mat[1] if mat
  end

  def parse_port
    mat = @args.match(/-p\s*(\d+)/)
    @port = mat[1].to_i if mat
  end
end

def run
  parser = Parser.new
  parser.parse
  if parser.is_server
    Server.new(parser.port).run
  else
    Client.new(parser.hostname, parser.port).run
  end
end

run
