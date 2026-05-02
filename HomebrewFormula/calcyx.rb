# Homebrew formula template for calcyx (CLI/TUI only).
#
# This file is the source-of-truth template kept in the main calcyx
# repository. It is NOT consumed directly by `brew tap`; instead
# `scripts/bump-formula.sh <tag>` substitutes the placeholder url/sha256
# for the given release tag and writes the result to
# `../homebrew-calcyx/Formula/calcyx.rb` (the actual tap repository that
# `brew tap ponzu840w/calcyx` reads).
#
# The GUI is shipped separately as a Homebrew Cask:
#   brew install --cask ponzu840w/calcyx/calcyx
# See HomebrewFormula/Casks/calcyx.rb for the Cask template.
#
# Edit this file when you want to change install steps, dependencies,
# caveats, etc. Don't bother updating the placeholder url/sha256 by
# hand: bump-formula.sh substitutes them on every release.
#
# Usage (end users, after the tap is published):
#   brew tap ponzu840w/calcyx
#   brew install calcyx                  # CLI/TUI
#   brew install --cask calcyx           # GUI (.app)

class Calcyx < Formula
  desc "Engineer's calculator (CLI/TUI). Scratchpad-style with hex/bin/ECC ops"
  homepage "https://github.com/ponzu840w/calcyx"
  # Placeholders. bump-formula.sh substitutes these per release tag.
  url "https://github.com/ponzu840w/calcyx/archive/refs/tags/v0.0.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "MIT"
  head "https://github.com/ponzu840w/calcyx.git", branch: "master"

  depends_on "cmake" => :build

  def install
    # mpdecimal と FTXUI は cmake/deps.cmake が ExternalProject_Add で
    # ソースから取り込み静的リンクするので brew 側で depends_on する必要はない。
    # CALCYX_BUILD_GUI=OFF で FLTK 依存ビルドを回避し CLI/TUI のみ作る。
    #
    # GitHub source tarball には .git/ がなく `git describe` が失敗して
    # 0.0.0-dev に落ちるので、 CMake にバージョンを明示で渡す。
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_INSTALL_PREFIX=#{prefix}",
                    "-DCALCYX_VERSION=#{version}",
                    "-DCALCYX_VERSION_FULL=#{version}",
                    "-DCALCYX_BUILD_GUI=OFF"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      The CLI/TUI binary is on PATH as `calcyx`.

      The FLTK GUI is distributed as a separate cask:
        brew install --cask ponzu840w/calcyx/calcyx
    EOS
  end

  test do
    # 基本評価
    assert_equal "8", shell_output("#{bin}/calcyx -e 3+5").strip
    # SI 接頭辞混じりの評価。 表示優先なので 1.5k のまま出る。
    assert_equal "1.5k", shell_output("#{bin}/calcyx -e 1.5k").strip
    # 10 進強制で実値が出ること (= 1.5k = 1500)
    assert_equal "1500", shell_output("#{bin}/calcyx -e 1.5k -o dec").strip
    # --version の出力に formula のバージョン番号が含まれること
    # (= CMakeLists.txt が -DCALCYX_VERSION_FULL を尊重している確認)。
    assert_match version.to_s, shell_output("#{bin}/calcyx --version")
  end
end
