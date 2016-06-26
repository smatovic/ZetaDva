# process epd file with node count for perft, opcode "acn"
# call:
# ruby ./acn.rb /home/srdja/Projects/ZetaDva/src/zetadva /home/srdja/Projects/ZetaDva/src/tmp.log /home/srdja/Projects/ZetaDva/docs/perft.epd

argengine=ARGV[0]
argfile=ARGV[1]
argepd=ARGV[2]

# time measurement 
class Time
  def to_ms
    (self.to_f * 1000.0).to_f
  end
end
#start_time = Time.now
#end_time = Time.now
#elapsed_time = end_time.to_ms - start_time.to_ms
#puts "elapsed seconds:" + (elapsed_time/1000).to_s;

# open temp file for communication with engine
f = File.open(argfile, "a+")
f.close

# test counters
passed = 0
tested = 0

# open io to engine, output piped to temp file
engineIO = IO.popen(argengine+' > '+argfile, 'w')

#   get each line in file
File.open(argepd, 'r') do |f1|
  # read fen string, depth, and nodecount from epd file
  while line = f1.gets  
    fen = "";
    id = line.scan(/id "([0-9a-zA-Z .()]+)";/).first.last.to_s;
    epd = Array;
    depth = 0;
    nodecount = 0;
    nc = 0;
    epd = line.scan(/[0-9a-zA-Z\/-]+/).to_a;
    # add fen position to string
    epd.each_with_index do |e, i|
      case e
        # add position
        when (/\//)
          fen+= " " + e
        # add color
        when "w"
          fen+= " " + e
          fen+= " " + epd[i+1].to_s # add castling
          fen+= " " + epd[i+2].to_s # add en passant square
        # add color
        when "b"
          fen+= " " + e
          fen+= " " + epd[i+1].to_s # add castling
          fen+= " " + epd[i+2].to_s # add en passant square
        # get depth
        when ("c0")
          depth = epd[i+1].to_i
        # get correct nodecount
        when ("c1")
          nodecount = epd[i+1].to_i
          break
      end
    end
    # wait for engine greetings 
    i=0
    while(i==0)
      sleep(0.1)
      i = IO.readlines(argfile).count
    end
    # init engine
#    engineIO.puts("log")
#    sleep(0.1)
    # init engine
    engineIO.puts("new")
    sleep(0.1)
    sleep(0.1)
    engineIO.puts("setboard "+fen)
    sleep(0.1)
    engineIO.puts("sd " + depth.to_s)
    sleep(0.1)
    i = IO.readlines(argfile).count
    start_time = Time.now
    engineIO.puts("perft")
    j = 0;
    # wait until result comes up in file
    check = 1;
    while (check==1)
      tmparr = IO.readlines(argfile)
      j = tmparr.count

      for k in i..j
        tmpstring = String.new
        tmpstring = tmparr[k].to_s
        # check for correct node count
        if (tmpstring.include? "nodecount:")
          nc = tmpstring.scan(/nodecount:([0-9]*),/).first.last.to_i;
          check = 0;
        end
      end
      sleep(0.01)
    end
    # time measurement
    end_time = Time.now
    elapsed_time = end_time.to_ms - start_time.to_ms
    if ((elapsed_time/1000)>=1)
      nps = (nodecount/(elapsed_time/1000)).to_i.to_s
    else
      nps = "inf"
    end
    tested+=1
    if nc == nodecount
      passed+=1
      puts id + ", depth: " + depth.to_s + ", enginenodecount: " + nc.to_s + "==" + nodecount.to_s + ", ca. seconds: " + (elapsed_time/1000).to_s.slice(0,4) + ", ca. nps: " + nps 
    else
      puts id + ", depth: " + depth.to_s + ", enginenodecount: " + nc.to_s + "!=" + nodecount.to_s + ", ca. seconds: " + (elapsed_time/1000).to_s.slice(0,4) + ", ca. nps: " + nps
    end
  end
puts "passed " + passed.to_s + " from " + tested.to_s
end

File.delete(argfile)

engineIO.puts("quit")
engineIO.close

