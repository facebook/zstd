class Zstd < Formula
  desc "Zstandard - Fast real-time compression algorithm"
  homepage "http://www.zstd.net/"
  url "https://github.com/Cyan4973/zstd/archive/v0.8.1.tar.gz"
  sha256 "4632bee45988dd0fe3edf1e67bdf0a833895cbb1a7d1eb23ef0b7d753f8bffdd"

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
