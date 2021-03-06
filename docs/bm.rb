# process epd file with opcodes avoid move and best move
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
    id = line.scan(/id "([0-9a-zA-Z .()]+)";/).first.last.to_s;
    epd = Array;
    epdmove = "";
    enginemove = "";
    depth = 0;
    nodecount = 0;
    nc = 0;
    epd = line.scan(/[0-9a-zA-Z\/-]+/).to_a;
    mode = "";
  
    # add fen position to string
    epd.each_with_index do |e, i|
      case e
        # add position
        when (/[0-9a-zA-Z]*\//)
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
        # get bestmove
        when ("bm")
          mode = "bm"
          epdmove = epd[i+1].to_s.chomp(';').strip
          break;
        # get bestmove
        when ("am")
          mode = "am"
          epdmove = epd[i+1].to_s.chomp(';').strip
          break;
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
#    engineIO.puts("epd")
#    sleep(0.1)
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
    check = 1;
    while (check==1)
      sleep(0.01)
      tmparr = IO.readlines(argfile)
      j = tmparr.count

      for k in i..j
        tmpstring = String.new
        tmpstring = tmparr[k].to_s
        if (tmpstring.include? "move" or tmpstring.include? "result")
          enginemove = tmpstring.slice(5,5).chomp
          check = 0;
        end
      end
    end
    end_time = Time.now
    elapsed_time = end_time.to_ms - start_time.to_ms
    totalelapsed_time = totalelapsed_time + elapsed_time
    # time measurement
    # check for correct bestove or avaid move
    tested+=1
    if (mode == "bm")
      if enginemove == epdmove
        passed+=1
        puts id + ", enginemove:" + enginemove + "==" + "bm:" + epdmove
      else
        puts id + ", enginemove:" + enginemove + "!=" + "bm:" + epdmove
      end
    end
    if (mode == "am")
      if enginemove != epdmove
        passed+=1
        puts id + ", enginemove:" + enginemove + "!=" + "am:" + epdmove
      else
        puts id + ", enginemove:" + enginemove + "==" + "am:" + epdmove
      end
    end
  end
puts "passed " + passed.to_s + " from " + tested.to_s + " in roughly " + (totalelapsed_time/1000).to_s.slice(0,4) + " seconds" 
end

File.delete(argfile)

engineIO.puts("quit")
engineIO.close

