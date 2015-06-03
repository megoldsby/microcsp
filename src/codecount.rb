
#
# Counts lines of code, excluding blank lines and comment lines.
#

def count_file(filename)
  k = 0
  File.open(filename, 'r') do |f|
    lines = f.readlines 
    lines.each do |ln|
      ln = ln.strip!
      next if ln == ''
      next if ln =~ /^(\/\/|\/\*)/
      k += 1
    end
  end
  k
end

files = []
ARGV.each { |f| files << f }
count = 0
files.each { |f| count += count_file(f) }
puts ("total nonblank, noncomment lines = #{count}")
