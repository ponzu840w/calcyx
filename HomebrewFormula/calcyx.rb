# Homebrew formula for calcyx (CLI/TUI + GUI).
#
# Usage:
#   brew tap ponzu840w/calcyx
#   brew install calcyx
#
# This formula builds both the integrated CLI/TUI binary (`calcyx`) and
# the FLTK-based GUI bundle (`calcyx.app`). The bundle ends up under
# the keg prefix (#{prefix}/calcyx.app); see `def caveats` for how to
# expose it under /Applications.
#
# url + sha256 lines must be updated for each new release.

class Calcyx < Formula
  desc "Engineer's calculator (CLI/TUI + GUI). Scratchpad-style with hex/bin/ECC ops"
  homepage "https://github.com/ponzu840w/calcyx"
  url "https://github.com/ponzu840w/calcyx/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "e67d638d7dd1ffe8c418660c048dca12414cf2d9f6d23cb6f2756683c090f816"
  license "MIT"
  head "https://github.com/ponzu840w/calcyx.git", branch: "master"

  depends_on "cmake" => :build

  def install
    # mpdecimal と FLTK / FTXUI は cmake/deps.cmake が ExternalProject_Add で
    # ソースから取り込み静的リンクするので brew 側で depends_on する必要はない。
    # CALCYX_BUILD_GUI / CALCYX_BUILD_CLI のデフォルトはどちらも ON。
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_INSTALL_PREFIX=#{prefix}"
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
    # SI 接頭辞
    assert_match "1500", shell_output("#{bin}/calcyx -e 1.5k").strip
    # --version が落ちないこと
    system bin/"calcyx", "--version"
    # GUI バンドルが配置されていること (= MACOSX_BUNDLE のヘルパが固定する path)
    assert_predicate prefix/"calcyx.app/Contents/MacOS/calcyx", :exist?
  end
end
