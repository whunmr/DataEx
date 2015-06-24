#!/usr/bin/env ruby

syms = []
total = 0
IO.popen("nm --demangle -S #{ARGV.join(' ')}").each_line { |line|
  addr, size, scope, name = line.split(' ', 4)
  next unless addr and size and scope and name
  name.chomp!
  addr = addr.to_i(16)
  size = size.to_i(16)
  total += size
  syms << [size, name]
}

syms.sort! { |a,b| b[0] <=> a[0] }

cumulative = 0.0
syms.each { |sym|
  size = sym[0]
  cumulative += size
  printf "%5.1f%%  %6s %s\n", cumulative / total * 100, size.to_s, sym[1]
}

printf "%5.1f%%  %6s %s\n", 100, total, "TOTAL"
