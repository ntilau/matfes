% PlotMesh(Mesh, Type)
% Type = 0: Numbering
% Type = 1: Labels
function PlotMesh(Mesh, Type)
if nargin < 2
    Type = 0;
end
switch Type
    case 0
        ele = Mesh.ele;
        node = Mesh.node;
        spig2 = Mesh.spig2;
        figure(1)
        trimesh(ele, node(:,1), node(:,2),'color','k');%, 'linewidth', 2)
        title('Elements');
        axis equal; axis tight; hold on;
        text( (node(ele(:,1),1) + node(ele(:,2),1) + ...
            node(ele(:,3),1) )/3, ...
        (node(ele(:,1),2) + node(ele(:,2),2) + ...
            node(ele(:,3),2) )/3, ...
        num2str((1:size(ele,1)).'),'color','b','FontSize',8, ...
            'HorizontalAlignment', 'center');
        figure(2)
        trimesh(ele, node(:,1), node(:,2), 'color','k');
        %, 'linewidth', 2)
        title('Edges');
        axis equal; axis tight; hold on;
        text( (node(spig2(:,1),1) + node(spig2(:,2),1))/2, ...
            (node(spig2(:,1),2) + node(spig2(:,2),2))/2, ...
        num2str((1:size(spig2,1)).'),'color','b','FontSize',8, ...
            'HorizontalAlignment', 'center');
        figure(3)
        plot(node(:,1), node(:,2), 'b.');%, 'markersize', 20);
        title('Nodes');
        axis equal; axis tight; hold on;
        text( node(:,1), node(:,2), num2str((1:size(node,1)).'), ...
            'FontSize',8, 'HorizontalAlignment', 'center');
    case 1
        ele = Mesh.ele;
        node = Mesh.node;
        spig2 = Mesh.spig2;
        elab = Mesh.elab;
        nlab = Mesh.nlab;
        slab = Mesh.slab;
        figure(1)
        trimesh(ele, node(:,1), node(:,2),'color','k');%, 'linewidth', 2)
        title('Labels');
        axis equal; axis tight; hold on;
        text( (node(ele(:,1),1) + node(ele(:,2),1) + ...
            node(ele(:,3),1) )/3, ...
        (node(ele(:,1),2) + node(ele(:,2),2) + ...
            node(ele(:,3),2) )/3, ...
        num2str(elab),'color','k','FontSize',8, ...
            'HorizontalAlignment', 'center');
        text( (node(spig2(:,1),1) + node(spig2(:,2),1))/2, ...
            (node(spig2(:,1),2) + node(spig2(:,2),2))/2, ...
            num2str(slab),'color','r','FontSize',8, ...
            'HorizontalAlignment', 'center');
%         text( node(:,1), node(:,2), num2str(nlab), 'color', 'b',...
%             'FontSize',8, 'HorizontalAlignment', 'center');
end