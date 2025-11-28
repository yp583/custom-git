import React, {useState, useEffect} from 'react';
import {Text, Box, useInput} from 'ink';

type Props = {
	content: string;
	filepath: string;
	focused: boolean;
	maxHeight?: number;
	maxWidth?: number;
};

export default function DiffViewer({
	content,
	filepath,
	focused,
	maxHeight = 20,
	maxWidth = 80,
}: Props) {
	const [scrollOffset, setScrollOffset] = useState(0);
	const [horizontalOffset, setHorizontalOffset] = useState(0);

	const lines = content.split('\n');
	const visibleLines = lines.slice(scrollOffset, scrollOffset + maxHeight);

	// Reset scroll when content changes
	useEffect(() => {
		setScrollOffset(0);
		setHorizontalOffset(0);
	}, [content]);

	// Handle j/k/h/l navigation
	useInput((input, key) => {
		if (!focused) return;
		if (input === 'j' || key.downArrow) {
			setScrollOffset(s => Math.min(Math.max(0, lines.length - maxHeight), s + 3));
		} else if (input === 'k' || key.upArrow) {
			setScrollOffset(s => Math.max(0, s - 3));
		} else if (input === 'h' || key.leftArrow) {
			setHorizontalOffset(h => Math.max(0, h - 10));
		} else if (input === 'l' || key.rightArrow) {
			setHorizontalOffset(h => h + 10);
		}
	});

	// Pad to fixed height to prevent re-render jitter
	const paddedLines: Array<{line: string; lineNum: number} | null> = [];
	for (let i = 0; i < maxHeight; i++) {
		if (i < visibleLines.length) {
			paddedLines.push({line: visibleLines[i]!, lineNum: scrollOffset + i + 1});
		} else {
			paddedLines.push(null);
		}
	}

	// Truncate and apply horizontal offset to a line
	const truncateLine = (line: string): string => {
		const shifted = line.slice(horizontalOffset);
		if (shifted.length > maxWidth) {
			return shifted.slice(0, maxWidth - 3) + '...';
		}
		return shifted;
	};

	return (
		<Box
			flexDirection="column"
			flexGrow={1}
			borderStyle="single"
			borderColor={focused ? 'cyan' : 'gray'}
			paddingX={1}
			height={maxHeight + 4}
		>
			<Text bold color="cyan">
				{filepath || 'No file selected'}
			</Text>
			<Box marginTop={1} flexDirection="column">
				{paddedLines.map((item, i) => {
					if (!item) {
						return <Text key={i}> </Text>;
					}

					let color: 'green' | 'red' | undefined;

					// Color additions green, deletions red (skip diff headers)
					if (item.line.startsWith('+') && !item.line.startsWith('+++')) {
						color = 'green';
					} else if (item.line.startsWith('-') && !item.line.startsWith('---')) {
						color = 'red';
					}

					return (
						<Text key={i} wrap="truncate">
							<Text dimColor>{String(item.lineNum).padStart(4)} </Text>
							<Text color={color}>{truncateLine(item.line)}</Text>
						</Text>
					);
				})}
			</Box>
			<Text dimColor>
				Lines {scrollOffset + 1}-
				{Math.min(scrollOffset + maxHeight, lines.length)} of {lines.length}
				{horizontalOffset > 0 && ` (col +${horizontalOffset})`}
			</Text>
		</Box>
	);
}
