/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   input.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jcologne <jcologne@student.42sp.org.br>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/11 17:44:29 by jcologne          #+#    #+#             */
/*   Updated: 2025/03/15 20:17:09 by jcologne         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

static void	handle_error(const char *message)
{
	perror(message);
	exit();
}

char	**parse_input(char *input)
{
	return (ft_split(input, 32));
}

char	*get_cmd_path(char *cmd)
{
	char	**paths;
	char	*path;
	int		i;

	paths = ft_split(getenv("PATH"), ':');
	if (!paths)
		handle_error("Error splitting PATH");
	i = 0;
	while (paths[i])
	{
		path = ft_strjoin(paths[i], "/");
		path = ft_strjoin(path, cmd);
		if (access(path, X_OK) == 0)
		{
			free(paths);
			return (path);
		}
		free(path);
		i++;
	}
	free(paths);
	return (NULL);
}
