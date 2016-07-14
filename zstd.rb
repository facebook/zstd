class Zstd < Formula
  desc "Zstandard - Fast real-time compression algorithm"
  homepage "http://www.zstd.net/"
  url "https://github.com/Cyan4973/zstd/archive/v0.7.3.tar.gz"
  sha256 "767da2a321b70d57a0f0776c39192a6c235c8f1fd7f1268eafde94a8869c3c71"

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
