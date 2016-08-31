class Zstd < Formula
  desc "Zstandard - Fast real-time compression algorithm"
  homepage "http://www.zstd.net/"
  url "https://github.com/Cyan4973/zstd/archive/v1.0.0.tar.gz"
  sha256 "197e6ef74da878cbf72844f38461bb18129d144fd5221b3598e973ecda6f5963"

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
