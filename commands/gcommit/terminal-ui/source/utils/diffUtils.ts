import type { DiffLine } from '../types.js';

/**
 * Parse a unified diff and reconstruct the full file view with diff markers.
 * Shows the complete file with additions (green), deletions (red), and context.
 */
export function parseUnifiedDiff(
  oldContent: string,
  newContent: string,
  diff: string
): DiffLine[] {
  const result: DiffLine[] = [];

  // If no diff, just show new content as context
  if (!diff.trim()) {
    const lines = newContent.split('\n');
    for (const line of lines) {
      result.push({ content: line, type: 'context' });
    }
    return result;
  }

  const oldLines = oldContent.split('\n');
  const diffLines = diff.split('\n');

  // Parse hunks from unified diff
  const hunks: Array<{
    oldStart: number;
    oldCount: number;
    newStart: number;
    newCount: number;
    lines: Array<{ type: 'context' | 'addition' | 'deletion'; content: string }>;
  }> = [];

  let currentHunk: typeof hunks[0] | null = null;

  for (const line of diffLines) {
    // Match hunk header: @@ -oldStart,oldCount +newStart,newCount @@
    const hunkMatch = line.match(/^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/);

    if (hunkMatch) {
      if (currentHunk) {
        hunks.push(currentHunk);
      }
      currentHunk = {
        oldStart: parseInt(hunkMatch[1]!, 10),
        oldCount: parseInt(hunkMatch[2] || '1', 10),
        newStart: parseInt(hunkMatch[3]!, 10),
        newCount: parseInt(hunkMatch[4] || '1', 10),
        lines: [],
      };
    } else if (currentHunk) {
      if (line.startsWith('+') && !line.startsWith('+++')) {
        currentHunk.lines.push({ type: 'addition', content: line.slice(1) });
      } else if (line.startsWith('-') && !line.startsWith('---')) {
        currentHunk.lines.push({ type: 'deletion', content: line.slice(1) });
      } else if (line.startsWith(' ')) {
        currentHunk.lines.push({ type: 'context', content: line.slice(1) });
      } else if (line === '') {
        // Empty context line
        currentHunk.lines.push({ type: 'context', content: '' });
      }
    }
  }

  if (currentHunk) {
    hunks.push(currentHunk);
  }

  // No hunks found, return new content as context
  if (hunks.length === 0) {
    const lines = newContent.split('\n');
    for (const line of lines) {
      result.push({ content: line, type: 'context' });
    }
    return result;
  }

  // Build full file view by interleaving old content with hunks
  let oldLineNum = 1;

  for (const hunk of hunks) {
    // Add unchanged lines before this hunk
    while (oldLineNum < hunk.oldStart) {
      const line = oldLines[oldLineNum - 1];
      if (line !== undefined) {
        result.push({ content: line, type: 'context' });
      }
      oldLineNum++;
    }

    // Add hunk lines
    for (const hunkLine of hunk.lines) {
      result.push({ content: hunkLine.content, type: hunkLine.type });
      if (hunkLine.type === 'context' || hunkLine.type === 'deletion') {
        oldLineNum++;
      }
    }
  }

  // Add remaining lines after last hunk
  while (oldLineNum <= oldLines.length) {
    const line = oldLines[oldLineNum - 1];
    if (line !== undefined) {
      result.push({ content: line, type: 'context' });
    }
    oldLineNum++;
  }

  return result;
}
