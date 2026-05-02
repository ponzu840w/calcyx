# Homebrew cask template for calcyx GUI (.app).
#
# Source-of-truth template kept in the main calcyx repository.
# `scripts/bump-formula.sh <tag>` substitutes the placeholder version /
# sha256 below for the given release tag and writes the result to
# `../homebrew-calcyx/Casks/calcyx.rb`.
#
# Unlike the Formula (which builds CLI from source), this Cask grabs the
# pre-built `.dmg` attached to the GitHub Release, so the dmg must be
# uploaded to the release page before bump-formula.sh can compute the
# correct sha256.
#
# Usage (end users, after the tap is published):
#   brew install --cask ponzu840w/calcyx/calcyx

cask "calcyx" do
  # Placeholders. bump-formula.sh substitutes these per release tag.
  version "0.0.0"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"

  url "https://github.com/ponzu840w/calcyx/releases/download/v#{version}/calcyx-mac-#{version}.dmg"
  name "calcyx"
  desc "Engineer's calculator (FLTK GUI)"
  homepage "https://github.com/ponzu840w/calcyx"

  # `.app` は CMake POST_BUILD で ad-hoc 再署名済み (= 「壊れている」
  # 起動拒否は出ない)。 ただし Apple Developer ID 署名 + notarize は
  # していないので、 初回起動時に Gatekeeper の「未確認の開発元」 警告が
  # 出る。 Finder で右クリック → 開く → 開く で 1 度だけ承認すれば以降は
  # 普通に起動できる。 caveats でも案内。
  app "calcyx.app"

  caveats <<~EOS
    On first launch macOS may show "未確認の開発元 (unverified developer)".
    Right-click calcyx.app in Finder → Open → Open to approve once.
    Subsequent launches work normally.
  EOS

  zap trash: [
    "~/Library/Application Support/calcyx",
    "~/Library/Preferences/com.ponzu840w.calcyx.plist",
  ]
end
