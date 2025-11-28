class CustomGit < Formula
  desc "AI-powered git commit commands with semantic clustering"
  homepage "https://github.com/yp583/custom-git"
  url "https://github.com/yp583/homebrew-custom-git/archive/refs/tags/v0.1.1.tar.gz"
  sha256 "1f3c4b14271ec0d86f5abab6b09d26a8b578af3391c9cc35b1267673b0c49e1e"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "node"
  depends_on "openssl@3"

  def install
    # Allow CPM/FetchContent to download dependencies during build
    ENV["HOMEBREW_ALLOW_FETCHCONTENT"] = "1"

    # Build gcommit (C++ executable)
    # Note: Not using std_cmake_args to avoid FetchContent trap - CPM needs to download deps
    cd "commands/gcommit" do
      system "cmake", "-S", ".", "-B", "build",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_VERBOSE_MAKEFILE=ON",
             "-DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}"
      system "cmake", "--build", "build", "--verbose"
      bin.install "build/git_gcommit.o"
    end

    # Build gcommit terminal-ui (Node.js CLI)
    cd "commands/gcommit/terminal-ui" do
      system "npm", "install"
      system "npm", "run", "build"
      bin.install "dist/cli.js" => "git-gcommit"
    end

    # Build mcommit (C++ executable)
    cd "commands/mcommit" do
      system "cmake", "-S", ".", "-B", "build",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_VERBOSE_MAKEFILE=ON",
             "-DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}"
      system "cmake", "--build", "build", "--verbose"
      bin.install "build/git_mcommit.o"
      bin.install "git-mcommit"
    end
  end

  def caveats
    <<~EOS
      To use these commands, set the OPENAI_API_KEY environment variable:
        export OPENAI_API_KEY="your-api-key"

      Available commands:
        git gcommit [threshold]  - Cluster staged changes and create semantic commits
        git mcommit [-i]         - Simple AI-generated commit message
    EOS
  end

  test do
    system bin/"git_gcommit.o", "--help"
    system bin/"git_mcommit.o", "--help"
  end
end
