# process sts lan epd file with best move list and scores
# call:
# ruby ./sts.rb /home/srdja/Projects/ZetaDva/src/zetadva /home/srdja/Projects/ZetaDva/src/tmp.log /home/srdja/Projects/ZetaDva/docs/STS1-STS15_LAN.EPD 2 128

require 'scanf'

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
found = 0;
tested = 0;
totalelapsed_time = 0;
score = 0;

# open io to engine, output piped to temp file
engineIO = IO.popen(argengine+' > '+argfile, 'w')

#   get each line in file
File.open(argepd, 'r') do |f1|
  # read fen string from epd file
  while line = f1.gets  
    fen = "";
    id = line.scan(/id "([0-9a-zA-Z .()\/]+)";/).first.last.to_s;
    epd = Array;
    enginemove = "";
    epd = line.scan(/[0-9a-zA-Z\/-]+/).to_a;
    mode = "";
    epdmoves = Array.new;
    epdscores = Array.new;
    outputstring = String.new; 
  
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
        # get epd scores 
        when ("c8")
          c8arr = line.scan( /c8 "([0-9 ]+)"/).first.last
          epdscores = c8arr.split(" ")
        # get epd bestmove 
        when ("c9")
          c9arr = line.scan( /c9 "([0-9a-z, ]+)"/).first.last
          epdmoves = c9arr.split(" ")
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
      sleep(0.01)
    end
    # time measurement
    end_time = Time.now
    elapsed_time = end_time.to_ms - start_time.to_ms
    totalelapsed_time = totalelapsed_time + elapsed_time
    # check for correct bestmove and add score
    tested+=1
    outputstring+= id.to_s + ", "
    outputstring+=  "enginemove:" + enginemove
    epdmoves.each_with_index do |move, index|
      if (move == enginemove)
        score+= epdscores[index].to_i
        found+=1
        outputstring+= "==" + "bm:" + move + ", score+:" + epdscores[index].to_s
      end
    end
    puts outputstring
  end
puts "found " + found.to_s + " from " + tested.to_s + " in roughly " + (totalelapsed_time/1000).to_s.slice(0,4) + " seconds"
puts "score: " + score.to_s
# Elo formula by Ferdinand Mosca
# src http://talkchess.com/forum/viewtopic.php?t=56653&highlight=sts+test+suite+engine+analysis+interface
puts "est Elo:" +  (44.523 * ((score.to_f/1000).to_f*100).to_f - 242.85).to_i.to_s
end

File.delete(argfile)

engineIO.puts("quit")
engineIO.close

