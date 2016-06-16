# process epd file with node count for perft, opcode "acn"
# call:
# ruby ./bm.rb /home/srdja/Projects/ZetaDva/src/zetadva /home/srdja/Projects/ZetaDva/src/tmp.log /home/srdja/Projects/ZetaDva/docs/kaufmann.epd 10 99

argengine=ARGV[0]
argfile=ARGV[1]
argepd=ARGV[2]
argsec=ARGV[3]
argdepth=ARGV[4]

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
totalelapsed_time = 0;

# open io to engine, output piped to temp file
engineIO = IO.popen(argengine+' > '+argfile, 'w')

#   get each line in file
File.open(argepd, 'r') do |f1|
  # read fen string, depth, and nodecount from epd file
  while line = f1.gets  
    fen = "";
    id = line.scan(/id \"[0-9a-zA-Z ]+\";/)[0].to_s;
    epd = Array;
    epdbm = "";
    bm = "";
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
        when "w" || "b" 
          fen+= " " + e
        # add no castle rights or no en passant
        when "-"
          fen+= " " + e
        # add present castle rights
        when (/[KQkq]/)
          fen+= " " + e
        # add present en passant target
        when (/[a-h][3-7]/)
          fen+= " " + e
        # get bestmove
        when ("bm")
          epdbm = epd[i+1].to_s.chomp(';')
      end
    end
    # wait for engine greetings 
    i=0
    while(i==0)
      sleep(0.1)
      i = IO.readlines(argfile).count
    end
    # init engine
    engineIO.puts("epd")
    sleep(0.1)
    engineIO.puts("new")
    sleep(0.1)
    engineIO.puts("setboard "+fen)
    sleep(0.1)
    engineIO.puts("sd " + argdepth.to_s)
    sleep(0.1)
    engineIO.puts("st " + argsec.to_s)
    sleep(0.1)
    i = IO.readlines(argfile).count
    start_time = Time.now
    engineIO.puts("go")
    j = 0;
    # wait until result comes up in file
    while (j<=i)
      sleep(0.001)
      j = IO.readlines(argfile).count
    end
    end_time = Time.now
    elapsed_time = end_time.to_ms - start_time.to_ms
    totalelapsed_time = totalelapsed_time + elapsed_time
    # time measurement
    # check for correct bestove
    bm = IO.readlines(argfile).last.chomp.to_s
    bm = bm.chomp(" ").to_s
    epdbm = epdbm.chomp(" ").to_s
    tested+=1
    if bm == epdbm
      passed+=1
      puts id + bm + "==" + epdbm
    else
      puts id + bm + "!=" + epdbm
    end
  end
p "passed " + passed.to_s + " from " + tested.to_s + " in roughly " + (totalelapsed_time/1000).to_s.slice(0,4) + " seconds" 
end

File.delete(argfile)

engineIO.puts("quit")
engineIO.close

