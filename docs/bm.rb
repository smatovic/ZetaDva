# process single epd file for opcode bm, bestmove, with SAN move notation
# call:
# ruby batchepd.rb /home/srdja/Projects/chess/ZetaDva/bin/zetadva0301.bin /home/srdja/Projects/chess/out.log /home/srdja/Projects/chess/ZetaDva/docs/kaufmann.epd

strengine=ARGV[0]
strlog=ARGV[1]
strepd=ARGV[2]

class Time
  def to_ms
    (self.to_f * 1000.0).to_i
  end
end
start_time = Time.now
end_time = Time.now
elapsed_time = end_time.to_ms - start_time.to_ms
puts elapsed_time

engine = IO.popen(strengine+' > '+strlog, 'w')
f = File.open(strlog, "a+")
engine.puts("new")
f.read

# open all files in directory 
#Dir.glob(strepd+"/**/*.epd") do |file|
#   get each line in file
#  File.open(file, 'r') do |f1|  
#   while line = f1.gets  
#      puts line  
#    end  
#  end  
#end

engine.puts("quit")
f.close

