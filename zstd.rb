class Zstd < Formula
  desc "Zstandard - Fast real-time compression algorithm"
  homepage "http://www.zstd.net/"
  url "https://github.com/Cyan4973/zstd/archive/v0.7.5.tar.gz"
  sha256 "6800defac9a93ddb1d9673d62c78526800df71cf9f353456f8e488ba4de51061"

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    (testpath/"input.txt").write("Hello, world." * 10)
    system "#{bin}/zstd", "input.txt", "-o", "compressed.zst"
    system "#{bin}/zstd", "--test", "compressed.zst"
    system "#{bin}/zstd", "-d", "compressed.zst", "-o", "decompressed.txt"
    system "cmp", "input.txt", "decompressed.txt"
  end
end
