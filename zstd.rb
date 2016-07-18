class Zstd < Formula
  desc "Zstandard - Fast real-time compression algorithm"
  homepage "http://www.zstd.net/"
  url "https://github.com/Cyan4973/zstd/archive/v0.7.4.tar.gz"
  sha256 "35ab3a5084d0194e9ff08e702edb6f507eab1bfb8c09c913639241cec852e2b7"

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
