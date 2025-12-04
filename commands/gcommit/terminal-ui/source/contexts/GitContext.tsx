import React, { createContext, useContext, useState, useEffect, useCallback, useMemo, ReactNode } from 'react';
import simpleGit, { SimpleGit } from 'simple-git';

interface GitState {
  git: SimpleGit;
  originalBranch: string;
  stagingBranch: string | null;
  stagedDiff: string;
}

interface GitContextValue extends GitState {
  createStagingBranch: () => Promise<string>;
  deleteStagingBranch: () => Promise<void>;
  mergeStagingBranch: () => Promise<void>;
  applyPatch: (patchContent: string) => Promise<void>;
  stageAll: () => Promise<void>;
  commit: (message: string) => Promise<void>;
  cleanup: () => Promise<void>;
}

const GitContext = createContext<GitContextValue | null>(null);

interface GitProviderProps {
  children: ReactNode;
}

export function GitProvider({ children }: GitProviderProps) {
  const [state, setState] = useState<GitState>({
    // @ts-ignore
    git: simpleGit(),
    originalBranch: '',
    stagingBranch: null,
    stagedDiff: '',
  });

  useEffect(() => {
    state.git.branch().then(b => {
      setState(s => ({ ...s, originalBranch: b.current }));
    });
  }, []);

  const createStagingBranch = useCallback(async (): Promise<string> => {
    const status = await state.git.status();
    const hasChanges = status.modified.length > 0 ||
                       status.not_added.length > 0 ||
                       status.deleted.length > 0 ||
                       status.staged.length > 0 ||
                       status.renamed.length > 0 ||
                       status.created.length > 0;

    if (hasChanges) {
      await state.git.stash(['push', '-u', '-m', 'gcommit-temp']);
    }

    const branchName = `gcommit-staging-${Date.now()}`;
    await state.git.checkoutLocalBranch(branchName);

    if (hasChanges) {
      try {
        await state.git.stash(['apply', '--index']);
      } catch (err: any) {
        console.error('Error applying stash with --index:', err);
        try {
          await state.git.stash(['apply']);
        } catch (err2: any) {
          console.error('Error applying stash:', err2);
        }
      }
    }

    const diff = await state.git.diff(['--cached']);
    setState(s => ({ ...s, stagedDiff: diff, stagingBranch: branchName }));

    await state.git.reset(['HEAD']);
    return branchName;
  }, [state.git]);

  const deleteStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      await state.git.checkout(state.originalBranch);
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const mergeStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      await state.git.checkout(state.originalBranch);
      await state.git.merge([state.stagingBranch]);
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const applyPatch = useCallback(async (patchPath: string) => {
    try {
      await state.git.raw(['apply', '--unidiff-zero', patchPath]);
    } catch (err: any) {
      console.error(`Failed to apply patch ${patchPath}:`, err);
      throw new Error(`Failed to apply patch ${patchPath}: ${err.message}`);
    }
  }, [state.git]);

  const stageAll = useCallback(async () => {
    await state.git.add('-A');
  }, [state.git]);

  const commit = useCallback(async (message: string) => {
    await state.git.commit(message);
  }, [state.git]);

  const cleanup = useCallback(async () => {
    if (state.stagingBranch) {
      try {
        await state.git.checkout(state.originalBranch);
        await state.git.deleteLocalBranch(state.stagingBranch, true);
      } catch (err: any) {
        console.error('Error cleaning up staging branch:', err);
      }
    }

    try {
      await state.git.stash(['apply', "--index"]);
    } catch (err: any) {
      console.error('Error dropping stash:', err);
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const value: GitContextValue = useMemo(() => ({
    ...state,
    createStagingBranch,
    deleteStagingBranch,
    mergeStagingBranch,
    applyPatch,
    stageAll,
    commit,
    cleanup,
  }), [state, createStagingBranch, deleteStagingBranch, mergeStagingBranch, applyPatch, stageAll, commit, cleanup]);

  return (
    <GitContext.Provider value={value}>
      {children}
    </GitContext.Provider>
  );
}

export function useGit(): GitContextValue {
  const context = useContext(GitContext);
  if (!context) {
    throw new Error('useGit must be used within a GitProvider');
  }
  return context;
}
