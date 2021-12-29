#!/usr/bin/env ruby
require 'open3'

class YCSB
  class << self
    def members
      %w[workload requestdistribution readallfields
         recordcount operationcount
         readproportion updateproportion scanproportion insertproportion
         fieldcount fieldlength]
    end

    def define_other_members
      members.each do |m|
        instance_variable_set("@#{m}", nil)
        define_method m do |val|
          "-p #{m}=#{val}"
        end
      end
    end
  end

  define_other_members

  def common_settings(records, ops)
    [workload('site.ycsb.workloads.CoreWorkload'),
     requestdistribution('zipfian'),
     readallfields('false'),
     recordcount(records),
     operationcount(ops),
     fieldcount(1),
     fieldlength(1)].join(' ')
  end

  def workloada(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(0.5),
     updateproportion(0.5),
     scanproportion(0),
     insertproportion(0)].join(' ')
  end

  def workloadb(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(0.95),
     updateproportion(0.05),
     scanproportion(0),
     insertproportion(0)].join(' ')
  end

  def workloadc(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(1),
     updateproportion(0),
     scanproportion(0),
     insertproportion(0)].join(' ')
  end

  def workloadd(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(0.95),
     updateproportion(0),
     scanproportion(0),
     insertproportion(0.05)].join(' ')
  end

  def workloade(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(0),
     updateproportion(0),
     scanproportion(0.95),
     insertproportion(0.05)].join(' ')
  end

  def workloadf(records, ops)
    [common_settings(records, ops),
     operationcount(ops),
     readproportion(0.5),
     updateproportion(0),
     scanproportion(0),
     insertproportion(0.5)].join(' ')
  end
end

def extract_data(data, out_file)
  File.open(out_file, 'w') do |f|
    pattern = /(\w+) .* (user\d+)/
    data.each_line do |line|
      mat = line.match(pattern)
      f.puts("#{mat[1]} #{mat[2]}") unless mat.nil?
    end
  end
end

def get_workloads(ycsb_dir, records, ops)
  Dir.chdir(ycsb_dir) do
    %w[a b c d e f].each do |t|
      params = YCSB.new.send("workload#{t}", records, ops)
      puts ">> Generating load workload #{t.upcase}"
      data, = Open3.capture3 "./bin/ycsb.sh load basic #{params}"
      puts ">> Extracting"
      extract_data(data, "./workloads/ycsb_load_#{t}.data")
      puts ">> Generating run workload #{t.upcase}"
      data, = Open3.capture3 "./bin/ycsb.sh run basic #{params}"
      puts ">> Extracting"
      extract_data(data, "./workloads/ycsb_run_#{t}.data")
    end
  end
end

get_workloads('third-party/ycsb-0.17.0', 10, 10)
