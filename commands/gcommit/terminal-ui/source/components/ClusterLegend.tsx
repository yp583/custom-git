import React from 'react';
import {Text, Box} from 'ink';

type Cluster = {
	id: number;
	message: string;
};

type Props = {
	clusters: Cluster[];
};

const COLORS: Array<'red' | 'cyan' | 'blue' | 'green' | 'yellow' | 'magenta' | 'white' | 'gray'> = [
	'red',
	'cyan',
	'blue',
	'green',
	'yellow',
	'magenta',
	'white',
	'gray',
];

export default function ClusterLegend({clusters}: Props) {
	return (
		<Box flexDirection="column" marginTop={1}>
			<Text bold>Clusters:</Text>
			{clusters.map((cluster, i) => (
				<Box key={cluster.id}>
					<Text color={COLORS[i % COLORS.length]}>● </Text>
					<Text dimColor>[{i}] </Text>
					<Text>{cluster.message}</Text>
				</Box>
			))}
		</Box>
	);
}
