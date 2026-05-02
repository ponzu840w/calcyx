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

  # `.app` は Apple Developer ID で署名・公証していないため、
  # quarantine 属性が付いたままだと macOS が「壊れている」 と表示して
  # 起動を拒否する。 quarantine: false で brew が install 時に
  # `xattr -d com.apple.quarantine` を実行し、 起動可能にする。
  # (= 「未確認の開発元」 警告には降格するが、 起動はできる)
  app "calcyx.app", quarantine: false

  zap trash: [
    "~/Library/Application Support/calcyx",
    "~/Library/Preferences/com.ponzu840w.calcyx.plist",
  ]
end
