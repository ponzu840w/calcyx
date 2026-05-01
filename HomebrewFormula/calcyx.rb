# Homebrew formula template for calcyx (CLI/TUI + GUI).
#
# This file is the source-of-truth template kept in the main calcyx
# repository. It is NOT consumed directly by `brew tap`; instead
# `scripts/bump-formula.sh <tag>` resolves the placeholder url/sha256
# below into concrete values for the given release tag and writes the
# result to `../homebrew-calcyx/Formula/calcyx.rb` (the actual tap
# repository that `brew tap ponzu840w/calcyx` reads).
#
# Edit this file when you want to change install steps, dependencies,
# caveats, etc. Don't bother updating the placeholder url/sha256 by
# hand: bump-formula.sh substitutes them on every release.
#
# Usage (end users, after the tap is published):
#   brew tap ponzu840w/calcyx
#   brew install calcyx
#
# Builds both the integrated CLI/TUI binary (`calcyx`) and the FLTK-based
# GUI bundle (`calcyx.app`). The bundle ends up under the keg prefix
# (#{prefix}/calcyx.app); see `def caveats` for how to expose it under
# /Applications.

class Calcyx < Formula
  desc "Engineer's calculator (CLI/TUI + GUI). Scratchpad-style with hex/bin/ECC ops"
  homepage "https://github.com/ponzu840w/calcyx"
  # Placeholders. bump-formula.sh substitutes these per release tag.
  url "https://github.com/ponzu840w/calcyx/archive/refs/tags/v0.0.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "MIT"
  head "https://github.com/ponzu840w/calcyx.git", branch: "master"

  depends_on "cmake" => :build

  def install
    # mpdecimal と FLTK / FTXUI は cmake/deps.cmake が ExternalProject_Add で
    # ソースから取り込み静的リンクするので brew 側で depends_on する必要はない。
    # CALCYX_BUILD_GUI / CALCYX_BUILD_CLI のデフォルトはどちらも ON。
    #
    # GitHub source tarball には .git/ がなく `git describe` が失敗して
    # 0.0.0-dev に落ちるので、 CMake にバージョンを明示で渡す。
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_INSTALL_PREFIX=#{prefix}",
                    "-DCALCYX_VERSION=#{version}",
                    "-DCALCYX_VERSION_FULL=#{version}"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      The CLI/TUI binary is on PATH as `calcyx`.

      The FLTK GUI bundle is installed under the keg as:
        #{opt_prefix}/calcyx.app

      To make it visible from Spotlight / Finder / Launchpad, link or copy
      it into /Applications:
        ln -sfn #{opt_prefix}/calcyx.app /Applications/calcyx.app
      or
        cp -R #{opt_prefix}/calcyx.app /Applications/
    EOS
  end

  test do
    # 基本評価
    assert_equal "8", shell_output("#{bin}/calcyx -e 3+5").strip
    # SI 接頭辞混じりの評価。 表示優先なので 1.5k のまま出る。
    assert_equal "1.5k", shell_output("#{bin}/calcyx -e 1.5k").strip
    # 10 進強制で実値が出ることを確認 (= 1.5k = 1500)。
    assert_equal "1500", shell_output("#{bin}/calcyx -e 1.5k -o dec").strip
    # --version の出力に formula のバージョン番号が含まれること
    # (= CMakeLists.txt が -DCALCYX_VERSION_FULL を尊重している確認)。
    assert_match version.to_s, shell_output("#{bin}/calcyx --version")
    # GUI バンドルが配置されていること (= MACOSX_BUNDLE のヘルパが固定する path)
    assert_predicate prefix/"calcyx.app/Contents/MacOS/calcyx", :exist?
  end
end
