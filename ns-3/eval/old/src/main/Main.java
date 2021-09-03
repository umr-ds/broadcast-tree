package main;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;

public class Main
{
	public static void main(String[] args) throws Exception
	{
		HashMap<String, ArrayList<String>> tree = new HashMap<>();
		
		String source = "";
		try(BufferedReader br = new BufferedReader(new FileReader("input.txt")))
		{
			String line;
			while((line = br.readLine()) != null)
			{
				line = line.substring(line.indexOf('['), line.length());
				String[] arr = line.split(" is ");
				
				String node = arr[0].substring(1, arr[0].length() - 1);
				
				if(source.equals(""))
					source = node;
				
				String parent = arr[1].substring(1, arr[1].length() - 1);
				
				ArrayList<String> list = tree.getOrDefault(parent, new ArrayList<>());
				list.add(node);
				tree.put(parent, list);
			}
		}
		
		System.out.println("<=========================Main Tree=========================>\n");
		for(String s : tree.get("ff:ff:ff:ff:ff:ff"))
		{
			if(s.equals(source))
			{
				Main.printTree(s, tree, 0);
				tree.get("ff:ff:ff:ff:ff:ff").remove(s);
				break;
			}
		}
		System.out.println("\n<=====================End of Main Tree======================>");
		
		
		for(String s : tree.get("ff:ff:ff:ff:ff:ff"))
		{
			System.out.println("\n<=========================Sub  Tree=========================>\n");
			Main.printTree(s, tree, 0);
		}
		tree.remove("ff:ff:ff:ff:ff:ff");
		
		System.out.println("\n<===========================================================>\n");
		
		String[] rest = tree.keySet().toArray(new String[tree.keySet().size()]);
		for(String key : rest)
		{
			if(tree.containsKey(key))
			{
				System.out.println("Node " + key + " remains");
				Main.printTree(key, tree, 0);
				System.out.println("\n");
			}
		}
	}
	
	public static void printTree(String node, HashMap<String, ArrayList<String>> tree, int depth)
	{
		for(int i = 0; i < depth; i++)
			System.out.print('\t');
		System.out.println("Child nodes of " + node + ":");
		
		ArrayList<String> list = tree.remove(node);
		if(list != null)
		{
			for(String s : list)
				Main.printTree(s, tree, depth + 1);
		}
	}
}