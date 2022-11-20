/******************************************************************************\
* Distance vector routing protocol without reverse path poisoning.             *
\******************************************************************************/

#include <stdlib.h>

#include "routing-simulator.h"

#include <stdio.h>

// Message format to send between nodes.
typedef struct {
    cost_t custos[MAX_NODES];
} message_t;

// State format.
typedef struct {
    cost_t estados[MAX_NODES][MAX_NODES];
} state_t;

int aux(state_t *estado_atual, bool notify_message){

  int path = COST_INFINITY;
  int atual = COST_INFINITY;
  int anterior = COST_INFINITY;
  int var = 0;

  for(int j = 0; j < MAX_NODES; j++){

    path = COST_INFINITY;
    atual = COST_INFINITY;
    anterior = COST_INFINITY;

    if(j != get_current_node()){

      if (notify_message){
        anterior = estado_atual->estados[get_current_node()][j];
      }

      for(int k = 0; k < MAX_NODES; k++){
        if(get_link_cost(k) != 0 && get_link_cost(k) + estado_atual->estados[k][j] < path){
          atual = k;
          path = get_link_cost(k) + estado_atual->estados[k][j];
        }
      }

      if(anterior != path && notify_message){
          var = 1;
      }

      set_route(j, atual, path);
      estado_atual->estados[get_current_node()][j] = path;
    }
  }
  return var;
}

void aux_send_message(state_t *estado_atual){
  for(int y = 0; y < MAX_NODES; y++){
    if(get_link_cost(y) != COST_INFINITY && y != get_current_node()){
      message_t *mensagem = (message_t *) malloc(sizeof(message_t));
      for(int x = 0; x < MAX_NODES; x++){
        mensagem->custos[x] = estado_atual->estados[get_current_node()][x];
      }

      send_message(y, mensagem);
    }
  }
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
  set_route(neighbor, neighbor, new_cost);
  state_t *estado_atual = (state_t *) get_state();

  if(estado_atual == NULL){

    estado_atual = (state_t *) malloc(sizeof(state_t));
    set_state(estado_atual);

    for(int x = 0; x < MAX_NODES; x++){
      for(int y = 0; y < MAX_NODES; y++){
        if (x == y){
          estado_atual->estados[x][y] = 0;
        }
        else{
          estado_atual->estados[x][y] = COST_INFINITY;
        }
      }
    }
  }

  if(new_cost <= estado_atual->estados[get_current_node()][neighbor]){
    estado_atual->estados[get_current_node()][neighbor] = new_cost;
  }

  else{
    aux(estado_atual, false);
  }
  aux_send_message(estado_atual);
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, void *message){

  state_t *estado_atual = (state_t *) get_state();
  message_t *mensagem = (message_t *)message;

  for(int x = 0; x < MAX_NODES; x++){
    estado_atual->estados[sender][x] = mensagem->custos[x];
  }

  if(aux(estado_atual, true) == 1){
    aux_send_message(estado_atual);
  }
}
